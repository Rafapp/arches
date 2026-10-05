#include "stdafx.hpp"
#include "include.hpp"
#include "intersect.hpp"
#include "custom-instr.hpp"

inline bool sphisect(const rtm::Sphere &sphere, const rtm::Ray &ray, rtm::Hit &hit)
{
#ifdef __riscv
	register float src0 asm("f0") = ray.o.x;
	register float src1 asm("f1") = ray.o.y;
	register float src2 asm("f2") = ray.o.z;
	register float src3 asm("f3") = ray.t_min;
	register float src4 asm("f4") = ray.d.x;
	register float src5 asm("f5") = ray.d.y;
	register float src6 asm("f6") = ray.d.z;
	register float src7 asm("f7") = ray.t_max;

	// cast the address of sphere first into a uint32_t
	uint32_t vaddr = static_cast<uint32_t>(reinterpret_cast<uint64_t>(&sphere));

	// then into a float so that it can be passed to the instruction in a FP register
	register float src8 asm("f8") = *reinterpret_cast<float *>(&vaddr);

	float t; // let the compiler assign a register for t

	asm volatile(
		".insn i 0xb, 0x7, %0, %1, 0\n\t"
		: "=f"(t)
		: "f"(src0), "f"(src1), "f"(src2),
		  "f"(src3),
		  "f"(src4), "f"(src5), "f"(src6),
		  "f"(src7),
		  "f"(src8)
	);

	hit.t = t;
	return hit.t < ray.t_max;
#else
	return rtm::intersect(sphere, ray, hit);
#endif
}

int main(void)
{
	const uint TILE_X    = 4;
	const uint TILE_Y    = 8;
	const uint TILE_SIZE = TILE_X * TILE_Y;
	static_assert(TILE_SIZE == 32);

	TRaXKernelArgs args = *(TRaXKernelArgs*)(void*)(TRAX_KERNEL_ARGS_ADDRESS);
	for(uint index = fchthrd(); index < args.framebuffer_size; index = fchthrd())
	{
		uint tile_id = index / TILE_SIZE;
		uint toffset = index % TILE_SIZE;
		uint tile_x  = tile_id % (args.framebuffer_width / TILE_X);
		uint tile_y  = tile_id / (args.framebuffer_width / TILE_X);
		uint x       = tile_x * TILE_X + toffset % TILE_X;
		uint y       = tile_y * TILE_Y + toffset / TILE_X;
		uint32_t fb_index = y * args.framebuffer_width + x;

		rtm::Ray ray = args.camera.generate_ray_through_pixel(x, y);
		rtm::Hit hit(ray.t_max, rtm::vec2(0.0f), ~0u);

		for(uint i = 0; i < args.sphere_count; ++i)
		{
			if(sphisect(args.spheres[i], ray, hit))
			{
				hit.id = i;
				ray.t_max = hit.t; // shrink the ray so farther spheres are rejected
			}
		}

		uint32_t color = (hit.id != ~0u) ? (rtm::RNG::hash(hit.id + 1U) | 0xff000000) : 0xff000000;
		args.framebuffer[fb_index] = color;
	}

	return 0;
}
