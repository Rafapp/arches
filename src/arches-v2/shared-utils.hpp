#pragma once
#include "stdafx.hpp"
#include <fstream>

#include "simulator/simulator.hpp"

#include "units/unit-dram.hpp"
#include "units/unit-cache.hpp"
#include "units/unit-crossbar.hpp"
#include "units/unit-buffer.hpp"
#include "units/unit-atomic-reg-file.hpp"
#include "units/unit-tile-scheduler.hpp"
#include "units/unit-sfu.hpp"
#include "units/unit-tp.hpp"

#include "util/elf.hpp"
#include "isa/riscv.hpp"
#include "rtm/rtm.hpp"

#if defined BUILD_PLATFORM_WINDOWS
	#include <Windows.h>
	#define MAX_FILENAME_LENGTH MAX_PATH
#elif defined BUILD_PLATFORM_LINUX
	#include <linux/limits.h>
	#define MAX_FILENAME_LENGTH FILENAME_MAX
#endif

char full_exe_name[MAX_FILENAME_LENGTH];
namespace Arches {

void set_full_exe_name(const char *name) {
	strcpy(full_exe_name, name);
}

std::string get_project_folder_path()
{
	// CHAR path[MAX_PATH];
	// GetModuleFileNameA(NULL, path, MAX_PATH);
	std::string executable_path(full_exe_name);
	return executable_path.substr(0, executable_path.rfind("build"));
}

template <typename T>
static T* write_array(Units::UnitMainMemoryBase* main_memory, size_t alignment, const T* data, size_t size, paddr_t& heap_address)
{
	paddr_t array_address = align_to(alignment, heap_address);
	heap_address = array_address + size * sizeof(T);
	main_memory->direct_write(data, size * sizeof(T), array_address);
	return reinterpret_cast<T*>(array_address);
}

template <typename T>
static T* write_vector(Units::UnitMainMemoryBase* main_memory, size_t alignment, const std::vector<T>& v, paddr_t& heap_address)
{
	return write_array(main_memory, alignment, v.data(), v.size(), heap_address);
}

template <typename T>
static T* write_array(uint8_t* main_memory, size_t alignment, const T* data, size_t size, paddr_t& heap_address)
{
	paddr_t array_address = align_to(alignment, heap_address);
	heap_address = array_address + size * sizeof(T);
	memcpy(main_memory + array_address, data, size * sizeof(T));
	return reinterpret_cast<T*>(array_address);
}

template <typename T>
static T* write_vector(uint8_t* main_memory, size_t alignment, const std::vector<T>& v, paddr_t& heap_address)
{
	return write_array(main_memory, alignment, v.data(), v.size(), heap_address);
}

template <typename T>
static T* write_array(Units::UnitMainMemoryBase** drams,  const Units::UnitCrossbar& xbar, size_t alignment, const T* data, size_t size, paddr_t& heap_address)
{
	paddr_t array_address = align_to(alignment, heap_address);
	heap_address = array_address + size * sizeof(T);
	for(uint i = 0; i < size * sizeof(T); ++i)
		drams[xbar.get_partition(array_address + i)]->direct_write((uint8_t*)data + i, 1, xbar.strip_partition_bits(array_address + i));
	return (T*)(array_address);
}

template <typename T>
static T* write_vector(Units::UnitMainMemoryBase** drams, const Units::UnitCrossbar& xbar, size_t alignment, const std::vector<T>& v, paddr_t& heap_address)
{
	return write_array(drams, xbar, alignment, v.data(), v.size(), heap_address);
}

template <class T, class L>
inline static L delta_log(L& master_log, std::vector<T*> units)
{
	L delta_log;
	for(uint i = 0; i < units.size(); ++i)
	{
		delta_log.accumulate(units[i]->log);
		units[i]->log.reset();
	}
	master_log.accumulate(delta_log);
	return delta_log;
}

template <class T, class L>
inline static L delta_log(L& master_log, T& unit)
{
	L delta_log = unit.log;
	master_log.accumulate(unit.log);
	unit.log.reset();
	return delta_log;
}

void print_header(std::string string, uint header_length = 80)
{
	uint spacers = string.length() < header_length ? header_length - string.length() : 0;
	printf("\n");
	for(uint i = 0; i < spacers / 2; ++i)
		printf("-");
	printf("%s", string.c_str());
	for(uint i = 0; i < (spacers + 1) / 2; ++i)
		printf("-");
	printf("\n");
}

const static std::vector<std::string> arch_names = {"TRaX", "STRaTA", "STRaTA-RT", "Dual-Streaming", "RIC"};


struct SceneConfig
{
	std::string name;
	rtm::vec3 cam_pos;
	rtm::vec3 cam_target;
	float focal_length;
};

const static std::vector<SceneConfig> scene_configs =
{
	{"cornell-box", rtm::vec3(0, 0.8, 1.8), rtm::vec3(0, 0.8, 0), 12.0f}, //CORNELLBOX

	{"sibenik", rtm::vec3(3.0, -13.0, 0.0), rtm::vec3(0, -12.0, 0), 12.0f}, 

	{"crytek-sponza", rtm::vec3(-900.6f, 150.8f, 120.74f), rtm::vec3(79.7f, 14.0f, -17.4f), 12.0f}, //CRYTEC SPONZA

	{"bistro-interior", rtm::vec3(-0.813307f, 2.0811f, -1.28115f), rtm::vec3(-0.813307f + 1.0f, 2.0811f, -1.28115f), 24.0f}, //CRYTEC SPONZA

	{"intel-sponza", rtm::vec3(-900.6f, 150.8f, 120.74f), rtm::vec3(79.7f, 14.0f, -17.4f), 12.0f}, //INTEL SPONZA
	
	{"sponza", rtm::vec3(0.0f, 2.0f, 0.0f), rtm::vec3(90.0f, 0.0f, -1.0f), 12.0f}, // SPONZA
	
	{"san-miguel", rtm::vec3(7.448, 1.014, 12.357), rtm::vec3(8.056, 1.04, 11.563), 12.0f}, //SAN_MIGUEL
	
	{"hairball", rtm::vec3(0, 0, 10), rtm::vec3(0, 0, 0), 24.0f}, //HAIRBALL

	{"bistro", rtm::vec3(-8.0, 2.0, 2.0), rtm::vec3(0.0f, 1.0f, -1.0f), 12.0f}, //BISTRO
	
	{"triangle", rtm::vec3(0.0, 0.0, 5.0), rtm::vec3(0.0f, 0.0f, 0.0f), 24.0f}, //TRIANGLE
	
	{"teapot", rtm::vec3(0.0, 0.0, 5.0), rtm::vec3(0.0f, 0.0f, 0.0f), 24.0f}, //TEAPOT
};


class SimulationConfig
{
public:
	struct Param
	{
		enum Type
		{
			INT,
			FLOAT,
			STRING,
		}
		type;


		union
		{
			int i;
			float f;
		};

		std::string s;

		Param() {};

		Param(const Param& other)
		{
			*this = other;
		}


		Param& operator=(const Param& other)
		{
			type = other.type;
			if(type == Param::Type::STRING)
			{
				s = std::string(other.s);
			}
			else
			{
				i = other.i;
			}
			return *this;
		}

		~Param()
		{

		}
	};

	rtm::Camera camera;

private:
	std::map<std::string, Param> _params;
	std::vector<std::pair<std::string, std::vector<std::string>>> _sweeps; //params with a list of values, one run per combination
	uint _run{0};

public:
	SimulationConfig(int argc, char* argv[])
	{
		//Simulation
		set_param("logging-interval", 10000);

		//Arch
		set_param("arch-name", "TRaX");
		set_param("max-rays", 64);

		//Hardware (defaults are RTX 2080, overridden by the hardware file and then by the command line)
		set_param("hardware", get_project_folder_path() + "src/arches-v2/hardware.cfg");
		set_param("core-clock-mhz", 1515);
		set_param("dram-clock-mhz", 3500);
		set_param("num-tms", 46);
		set_param("num-tps", 64);
		set_param("num-threads", 8);
		set_param("stack-size", 1024);
		set_param("num-partitions", 8);
		set_param("dram-config", "gddr6_14000_config.yaml");
		set_param("dram-latency", 92);
		set_param("l2-size-kb", 512);
		set_param("l2-associativity", 16);
		set_param("l2-slices", 4);
		set_param("l2-mshr", 192);
		set_param("l2-subentries", 4);
		set_param("l2-latency", 160);
		set_param("l1d-size-kb", 64);
		set_param("l1d-associativity", 32);
		set_param("l1d-banks", 16);
		set_param("l1d-mshr", 256);
		set_param("l1d-subentries", 16);
		set_param("l1d-latency", 20);

		//Workload
		set_param("dataset-dir", "./datasets");
		set_param("scene-name", "sponza");
		set_param("framebuffer-width", 1024);
		set_param("framebuffer-height", 1024);
		set_param("pregen-rays", 0);
		set_param("pregen-bounce", 1);
		set_param("bvh-preset", 0);
		set_param("bvh-merging", 0);

		parse_args(argc, argv); //only to find --hardware
		parse_file(get_string("hardware"));
		parse_args(argc, argv);
	}

	void parse_args(int argc, char* argv[])
	{
		for(uint i = 1; i < argc; ++i)
		{
			std::string arg(argv[i]);
			size_t start_pos = arg.find("--");
			size_t split_pos = arg.find("=");
			if(start_pos == std::string::npos || split_pos == std::string::npos) continue;

			//--xxx=yyy
			start_pos += 2;
			std::string key = arg.substr(start_pos, split_pos - start_pos);
			split_pos++;
			std::string value = arg.substr(split_pos, arg.size() - split_pos);

			parse_param(key, value);
		}
	}

	//one "key = value" per line, # starts a comment
	void parse_file(const std::string& path)
	{
		std::ifstream file(path);
		for(std::string line; std::getline(file, line);)
		{
			line = line.substr(0, line.find('#'));
			line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());
			size_t split_pos = line.find("=");
			if(split_pos == std::string::npos) continue;

			parse_param(line.substr(0, split_pos), line.substr(split_pos + 1));
		}
	}

	//applies the next combination of swept params, returns false once all of them have been run
	bool next_run()
	{
		uint num_runs = 1;
		for(auto& sweep : _sweeps) num_runs *= sweep.second.size();
		if(_run >= num_runs) return false;

		uint index = _run;
		for(auto& sweep : _sweeps)
		{
			set_value(sweep.first, sweep.second[index % sweep.second.size()]);
			index /= sweep.second.size();
		}
		set_param("image-name", num_runs > 1 ? "out-" + std::to_string(_run) + ".png" : std::string("out.png"));
		_run++;

		set_param("arch-id", -1);
		for(int i = 0; i < arch_names.size(); ++i)
			if(get_string("arch-name").compare(arch_names[i]) == 0)
				set_param("arch-id", i);
		_assert(get_int("arch-id") != -1);

		set_param("scene-id", -1);
		for(int i = 0; i < scene_configs.size(); ++i)
			if(get_string("scene-name").compare(scene_configs[i].name) == 0)
				set_param("scene-id", i);
		_assert(get_int("scene-id") != -1);

		uint scene_id = get_int("scene-id");
		camera = rtm::Camera(get_int("framebuffer-width"), get_int("framebuffer-height"), scene_configs[scene_id].focal_length, scene_configs[scene_id].cam_pos, scene_configs[scene_id].cam_target);

		print();
		return true;
	}

	int get_int(const std::string& key) const
	{
		const auto& a = _params.find(key);
		_assert(a != _params.end());
		_assert(a->second.type == Param::Type::INT);
		return a->second.i;
	}

	float get_float(const std::string& key) const
	{
		const auto& a = _params.find(key);
		_assert(a != _params.end());
		_assert(a->second.type == Param::Type::FLOAT);
		return a->second.f;
	}

	std::string get_string(const std::string& key) const
	{
		const auto& a = _params.find(key);
		_assert(a != _params.end());
		_assert(a->second.type == Param::Type::STRING);
		return a->second.s;

	}


	void set_param(const std::string& key, int value)
	{
		_params[key].type = Param::Type::INT;
		_params[key].i = value;
	}

	void set_param(const std::string& key, float value)
	{
		_params[key].type = Param::Type::FLOAT;
		_params[key].f = value;
	}

	void set_param(const std::string& key, const std::string& value)
	{
		_params[key].type = Param::Type::STRING;
		_params[key].s = std::string(value);
	}

	//a comma separated list (e.g. 1,2,4,8) sweeps the param
	void parse_param(const std::string& key, const std::string& str)
	{
		std::erase_if(_sweeps, [&](const auto& sweep) { return sweep.first == key; });
		if(_params.count(key) && str.find(',') != std::string::npos)
		{
			std::vector<std::string> values;
			for(size_t pos = 0, end; pos <= str.size(); pos = end + 1)
			{
				end = std::min(str.find(',', pos), str.size());
				values.push_back(str.substr(pos, end - pos));
			}
			_sweeps.push_back({key, values});
		}
		else set_value(key, str);
	}

	void set_value(const std::string& key, const std::string& str)
	{
		if(_params.count(key))
		{
			if(_params[key].type == Param::Type::INT)
			{
				_params[key].i = std::stoi(str);
			}
			else if(_params[key].type == Param::Type::FLOAT)
			{
				_params[key].f = std::stof(str);
			}
			else if(_params[key].type == Param::Type::STRING)
			{
				_params[key].s = std::string(str);
			}
		}
		else
		{
			printf("Invalid Parameter!!!: %s\n", key.c_str());
		}
	}

	void print()
	{
		printf("Simulation Parameters\n");
		for(std::pair<const std::string, Param>& a : _params)
		{
			if(a.second.type == Param::Type::INT)
			{
				printf("%s: %d\n", a.first.c_str(), a.second.i);
			}
			else if(a.second.type == Param::Type::FLOAT)
			{
				printf("%s: %f\n", a.first.c_str(), a.second.f);
			}
			else if(a.second.type == Param::Type::STRING)
			{
				printf("%s: %s\n", a.first.c_str(), a.second.s.c_str());
			}
		}
		printf("\n");
	}
};

}
