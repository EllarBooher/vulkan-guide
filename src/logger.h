#pragma once

#include <chrono>
#include <string_view>
#include <iostream>

#include "fmt/core.h"
#include "fmt/os.h"
#include "fmt/color.h"
#include "fmt/chrono.h"

#define VK_CHECKS(results) logger::vk_checks(results);
#define VK_CHECK(result) logger::vk_check(result);
#define VKB_CHECK(result) { \
		if (!result.has_value()) \
		{	\
			logger::vk_check(result.vk_result());\
		}\
	}
#define LOG_VKRESULT(result) { \
		logger::print_time(); \
		logger::print("VkResult: {}", result); \
	}

#define LOG_FATAL(message,...) logger::log_fatal(message, ##__VA_ARGS__);
#define LOG_ERROR(message,...) logger::log_error(message, ##__VA_ARGS__);
#define LOG_WARNING(message,...) logger::log_warning(message, ##__VA_ARGS__);
#define LOG_INFO(message,...) logger::log_info(message, ##__VA_ARGS__);
#define LOG_SUCCESS(message,...) logger::log_success(message, ##__VA_ARGS__);

namespace logger
{
	constexpr std::string_view _log_tag_format = "{:>10} ";

	enum class LogType {
		Fatal,
		Error,
		Warning,
		Info,
		Success
	};

	template <typename... Args>
	inline static void print(std::string_view message, Args... args)
	{
		fmt::print((message), args...);
		fmt::print("\n");
	}

	inline static void print_time()
	{
		static std::chrono::time_point<std::chrono::steady_clock> start = std::chrono::steady_clock::now();
		std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();

		std::chrono::duration timestamp = now - start;

		fmt::print("[{:<12%H:%M:%S}]", now - start, 1);
	}

	template <typename... Args>
	inline static void log_fatal(std::string_view message, Args... args)
	{
		print_time();
		fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold, _log_tag_format, "[FATAL]");
		print(message, args...);
	}
	template <typename... Args>
	inline static void log_error(std::string_view message, Args... args)
	{
		print_time();
		fmt::print(fg(fmt::color::crimson), _log_tag_format, "[ERROR]");
		print(message, args...);

	}
	template <typename... Args>
	inline static void log_warning(std::string_view message, Args... args)
	{
		print_time();
		fmt::print(fg(fmt::color::yellow), _log_tag_format, "[WARNING]");
		print(message, args...);
	}
	template <typename... Args>
	inline static void log_info(std::string_view message, Args... args)
	{
		print_time();
		fmt::print(fg(fmt::color::beige), _log_tag_format, "[INFO]");
		print(message, args...);
	}
	template <typename... Args>
	inline static void log_success(std::string_view message, Args... args)
	{
		print_time();
		fmt::print(fg(fmt::color::light_green), _log_tag_format,"[SUCCESS]");
		print(message, args...);
	}

	inline static void vk_check(VkResult result)
	{
		if (result != VK_SUCCESS)
		{
			log_error("Failed VkResult: {}", result);
		}
	}
	inline static void vk_checks(std::vector<VkResult> results)
	{
		for (int i = 0; i < results.size(); i++)
		{
			auto result = results[i];
			if (result == VK_SUCCESS) continue;

			log_error("Failed VkResult: {}", result);
		}
	}
}