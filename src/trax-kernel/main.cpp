#include "stdafx.hpp"
#include "include.hpp"
#include "custom-instr.hpp"
#include "intersect.hpp"

inline uint pack_color_rgb(const rtm::vec3& c)
{
	// clamp to [0,1] first, otherwise HDR values wrap around
	float r = c.x < 0.0f ? 0.0f : (c.x > 1.0f ? 1.0f : c.x);
	float g = c.y < 0.0f ? 0.0f : (c.y > 1.0f ? 1.0f : c.y);
	float b = c.z < 0.0f ? 0.0f : (c.z > 1.0f ? 1.0f : c.z);

	uint R = (uint)(r * 255.0f + 0.5f);
	uint G = (uint)(g * 255.0f + 0.5f);
	uint B = (uint)(b * 255.0f + 0.5f);

	return 0xff000000u | (B << 16) | (G << 8) | R;
}

inline uint pack_color_rgba(const rtm::vec4& c)
{
	// Clamp to [0,1] first, otherwise HDR values wrap around
	float r = c.x < 0.0f ? 0.0f : (c.x > 1.0f ? 1.0f : c.x);
	float g = c.y < 0.0f ? 0.0f : (c.y > 1.0f ? 1.0f : c.y);
	float b = c.z < 0.0f ? 0.0f : (c.z > 1.0f ? 1.0f : c.z);

	uint R = (uint)(r * 255.0f + 0.5f);
	uint G = (uint)(g * 255.0f + 0.5f);
	uint B = (uint)(b * 255.0f + 0.5f);

	return 0xff000000u | (B << 16) | (G << 8) | R;
}

int main()
{
	const TRaXKernelArgs args = *(TRaXKernelArgs*)(TRAX_KERNEL_ARGS_ADDRESS);

	const uint TILE_WIDTH  = 8;
	const uint TILE_HEIGHT = 4;
	const uint TILE_SIZE   = TILE_WIDTH * TILE_HEIGHT;
	static_assert(TILE_SIZE == 32);

	const uint fb_width  = args.framebuffer_width;
	const uint fb_height = args.framebuffer_height;

	for (uint tid = fchthrd(); tid < args.framebuffer_size; tid = fchthrd())
	{
		uint tile_id = tid / TILE_SIZE;		
		uint toffset = tid % TILE_SIZE;

		uint tile_x = tile_id % (args.framebuffer_width / TILE_WIDTH);
		uint tile_y = tile_id / (args.framebuffer_width / TILE_WIDTH);

		uint x = tile_x * TILE_WIDTH + toffset % TILE_WIDTH;
		uint y = tile_y * TILE_HEIGHT + toffset / TILE_WIDTH;

		uint fb_index = y * fb_width + x;

		// 1. Trace ray
		rtm::Ray ray = args.camera.generate_ray_through_pixel(x, y);
		rtm::Hit hit;
		hit.t = ray.t_max;
		hit.bc = rtm::vec2(0.0f);
		hit.id = ~0U;
		_traceray<0x0U>(0, ray, hit);

		// 2. Hit shader
		uint mat_id = uint(-1);
		if (hit.t < ray.t_max) {
			mat_id = args.material_indices[hit.id];

			if (mat_id != uint(-1))
			{
				rtm::Material& mat = args.materials[mat_id];

				rtm::vec4 albedo = rtm::vec4(1.0f, 0.0f, 1.0f, 1.0f);
				if (mat.use_am)
				{
					// Find barycentrics (only valid when the mesh has UVs)
					rtm::uvec3 tci = args.tex_coord_indices[hit.id];
					rtm::vec2 uv =
						args.tex_coords[tci[0]] * hit.bc[0] +
						args.tex_coords[tci[1]] * hit.bc[1] +
						args.tex_coords[tci[2]] * (1.0f - hit.bc[0] - hit.bc[1]);

					albedo = sample2d(&mat.albedo_texture, uv);
				}
				else {
					// (No swizzling supported?)
					albedo = rtm::vec4(mat.albedo.x, mat.albedo.y, mat.albedo.z, 1.0f);
				}
				args.framebuffer[fb_index] = pack_color_rgba(albedo);
			}
			else {
				rtm::vec3 magenta = rtm::vec3(1.0f, 0.0f, 1.0f);
				args.framebuffer[fb_index] = pack_color_rgb(magenta);
			}
		}
		// 3. Miss shader
		else {
			rtm::vec3 sky_color = rtm::vec3(0.5f, 0.7f, 1.0f);
			args.framebuffer[fb_index] = pack_color_rgb(sky_color);
		}
	}
	return 0;
}