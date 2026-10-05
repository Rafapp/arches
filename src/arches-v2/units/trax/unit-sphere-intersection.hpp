#pragma once
#include "stdafx.hpp"
#include "../unit-base.hpp"
#include "../unit-memory-base.hpp"

namespace Arches {
namespace Units {
namespace TRaX {

class UnitSphereIntersection : public UnitMemoryBase
{
public:
	struct Configuration
	{
		uint            num_pipelines{1};
		uint            pipeline_depth{1};
		uint            cpi{1};
		uint            num_clients{1};
		UnitMemoryBase* cache{nullptr};
		uint            cache_port{0};
	};

private:
	UnitMemoryBase *cache;
	uint cache_port;

	struct IntersectRequest
	{
		rtm::Ray ray;
		rtm::Sphere sphere;
		MemoryRequest primary_req;
	};

	std::queue<MemoryRequest> load_sphere_requests;
	std::unordered_map<uint32_t, MemoryRequest> wait_pool;
	std::queue<IntersectRequest> intersect_requests;
	std::vector<LatencyFIFO<IntersectRequest>> intersect_pipelines;
	uint cpi;

	RequestCrossBar request_network;
	ReturnCrossBar return_network;

public:
	UnitSphereIntersection(const Configuration &config) :
		cache(config.cache), cache_port(config.cache_port),
		request_network(config.num_clients, config.num_pipelines, 1, 1),
		return_network(config.num_pipelines, config.num_clients, 1),
		intersect_pipelines(config.num_pipelines, {config.pipeline_depth}),
		cpi(config.cpi)
	{
	}

	/* UnitBase interface */

	void clock_rise(void) override
	{
		request_network.clock();

		// receive return from cache and begin ray-sphere intersection from
		// next cycle
		if(cache->return_port_read_valid(cache_port))
		{
			for( uint pipeline_index = 0;
				 pipeline_index < intersect_pipelines.size();
				 ++pipeline_index )
			{
				if(intersect_pipelines[pipeline_index].is_write_valid())
				{
					MemoryReturn ret = cache->read_return(cache_port);
					MemoryRequest primary_req = wait_pool[ret.dst.raw];
					primary_req.dst.pop(8); // un-hash dst

					wait_pool.erase(ret.dst.raw);

					IntersectRequest intersect_req;
					memcpy(
						&intersect_req.primary_req,
						&primary_req,
						sizeof(MemoryRequest)
					);
					memcpy(
						&intersect_req.ray,
						primary_req.data,
						sizeof(rtm::Ray)
					);
					memcpy(
						&intersect_req.sphere,
						ret.data,
						sizeof(rtm::Sphere)
					);

					intersect_pipelines[pipeline_index].write(intersect_req);
					log.spheres++;
					break;
				}
			}
		}

		// receive requests from clients and enqueue a cache request for
		// the sphere
		for( uint pipeline_index = 0;
			 pipeline_index < intersect_pipelines.size();
			 ++pipeline_index )
		{
			if( load_sphere_requests.size() <= 0 &&
				request_network.is_read_valid(pipeline_index) )
			{
				MemoryRequest primary_req = request_network.read(pipeline_index);

				// hash dst to use as a key in waiting pool
				primary_req.dst.push(primary_req.port, 8);

				MemoryRequest load_sphere_req;
				load_sphere_req.type  = MemoryRequest::Type::LOAD;
				load_sphere_req.vaddr = primary_req.vaddr;
				load_sphere_req.size  = sizeof(rtm::Sphere);
				load_sphere_req.port  = cache_port;
				load_sphere_req.dst   = primary_req.dst;

				load_sphere_requests.push(load_sphere_req);
				wait_pool[load_sphere_req.dst.raw] = primary_req;
			}
		}
	}

	void clock_fall(void) override
	{
		// send return to client
		for ( uint pipeline_index = 0;
			  pipeline_index < intersect_pipelines.size();
			  ++pipeline_index )
		{
			intersect_pipelines[pipeline_index].clock();

			if( intersect_pipelines[pipeline_index].is_read_valid() &&
				return_network.is_write_valid(pipeline_index) )
			{
				const IntersectRequest &intersect_req =
					intersect_pipelines[pipeline_index].read();

				rtm::Hit hit;
				hit.t = intersect_req.ray.t_max;
				rtm::intersect(intersect_req.sphere, intersect_req.ray, hit);
				log.intersections++;

				MemoryReturn ret(intersect_req.primary_req);
				ret.size = sizeof(float);
				((float *)(ret.data))[0] = hit.t;
				return_network.write(ret, pipeline_index);
			}
		}

		// send sphere requests to cache
		if( load_sphere_requests.size() &&
			cache->request_port_write_valid(cache_port) )
		{
			cache->write_request(load_sphere_requests.front());
			load_sphere_requests.pop();
		}

		return_network.clock();
	}

	/* UnitMemoryBase interface */

	// Can be used in client's Send phase (on clock fall) only
	virtual bool request_port_write_valid(uint port_index) override
	{
		return request_network.is_write_valid(port_index);
	}

	virtual void write_request(const MemoryRequest &request) override
	{
		request_network.write(request, request.port);
	}

	// Can be used in client's Receive phase (on clock rise) only
	virtual bool return_port_read_valid(uint port_index) override
	{
		return return_network.is_read_valid(port_index);
	}

	virtual const MemoryReturn &peek_return(uint port_index) override
	{
		return return_network.peek(port_index);
	}

	virtual const MemoryReturn read_return(uint port_index) override
	{
		return return_network.read(port_index);
	}

	/* Log */
	class Log
	{
		const static uint NUM_COUNTERS = 4;

	public:
		union
		{
			struct
			{
				uint64_t spheres;
				uint64_t intersections;
			};
			uint64_t counters[NUM_COUNTERS];
		};

	public:
		Log() { reset(); }

		void reset()
		{
			for(uint i = 0; i < NUM_COUNTERS; ++i)
				counters[i] = 0;
		}

		void accumulate(const Log& other)
		{
			for(uint i = 0; i < NUM_COUNTERS; ++i)
				counters[i] += other.counters[i];
		}

		void print(cycles_t cycles, uint units = 1)
		{
			printf("Sphere Loads: %lld\n", spheres / units);
			printf("Intersections: %lld\n", intersections / units);
		}
	}
	log;
};

} } }
