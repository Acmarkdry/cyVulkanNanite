#pragma once
#include <vulkan/vulkan.hpp>

#include "RAIIHandle.h"

namespace cyRenderGraph
{
	namespace Colors
	{
		//https://flatuicolors.com/palette/defo
#define RGBA_LE(col) (((col & 0xff000000) >> (3 * 8)) + ((col & 0x00ff0000) >> (1 * 8)) + ((col & 0x0000ff00) << (1 * 8)) + ((col & 0x000000ff) << (3 * 8)))
		static constexpr uint32_t turqoise = RGBA_LE(0x1abc9cffu);
		static constexpr uint32_t greenSea = RGBA_LE(0x16a085ffu);

		static constexpr uint32_t emerald = RGBA_LE(0x2ecc71ffu);
		static constexpr uint32_t nephritis = RGBA_LE(0x27ae60ffu);

		static constexpr uint32_t peterRiver = RGBA_LE(0x3498dbffu); //blue
		static constexpr uint32_t belizeHole = RGBA_LE(0x2980b9ffu);

		static constexpr uint32_t amethyst = RGBA_LE(0x9b59b6ffu);
		static constexpr uint32_t wisteria = RGBA_LE(0x8e44adffu);

		static constexpr uint32_t sunFlower = RGBA_LE(0xf1c40fffu);
		static constexpr uint32_t orange = RGBA_LE(0xf39c12ffu);

		static constexpr uint32_t carrot = RGBA_LE(0xe67e22ffu);
		static constexpr uint32_t pumpkin = RGBA_LE(0xd35400ffu);

		static constexpr uint32_t alizarin = RGBA_LE(0xe74c3cffu);
		static constexpr uint32_t pomegranate = RGBA_LE(0xc0392bffu);

		static constexpr uint32_t clouds = RGBA_LE(0xecf0f1ffu);
		static constexpr uint32_t silver = RGBA_LE(0xbdc3c7ffu);
		static constexpr uint32_t imguiText = RGBA_LE(0xF2F5FAFFu);
	}

	struct ProfilerTask
	{
		double startTime;
		double endTime;
		std::string name;
		uint32_t color;

		double GetLength()
		{
			return endTime - startTime;
		}
	};

	class CpuProfiler
	{
	public:
		CpuProfiler()
		{
			frameIndex = 0;
		}

		size_t StartTask(std::string taskName, uint32_t taskColor)
		{
			legit::ProfilerTask task;
			task.color = taskColor;
			task.name = taskName;
			task.startTime = GetCurrFrameTimeSeconds();
			task.endTime = -1.0;
			size_t taskId = profilerTasks.size();
			profilerTasks.push_back(task);
			return taskId;
		}

		ProfilerTask EndTask(size_t taskId)
		{
			assert(profilerTasks.size() == taskId + 1 && profilerTasks.back().endTime < 0.0);
			profilerTasks.back().endTime = GetCurrFrameTimeSeconds();
			return profilerTasks.back();
		}

		size_t StartFrame()
		{
			profilerTasks.clear();
			frameStartTime = hrc::now();
			return frameIndex;
		}

		void EndFrame(size_t frameId)
		{
			assert(frameId == frameIndex);
			frameIndex++;
		}

		const std::vector<ProfilerTask>& GetProfilerTasks()
		{
			return profilerTasks;
		}

	private:
		double GetCurrFrameTimeSeconds()
		{
			return static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(hrc::now() - frameStartTime).count()) / 1e6;
		}

		struct TaskHandleInfo
		{
			TaskHandleInfo(CpuProfiler* _profiler, size_t _taskId)
			{
				this->profiler = _profiler;
				this->taskId = _taskId;
			}

			void Reset()
			{
				profiler->EndTask(taskId);
			}

			CpuProfiler* profiler;
			size_t taskId;
		};

		struct FrameHandleInfo
		{
			FrameHandleInfo(CpuProfiler* _profiler, size_t _frameId)
			{
				this->profiler = _profiler;
				this->frameId = _frameId;
			}

			void Reset()
			{
				profiler->EndFrame(frameId);
			}

			CpuProfiler* profiler;
			size_t frameId;
		};

	public:
		using ScopedTask = RAIIHandle<TaskHandleInfo, CpuProfiler>;

		ScopedTask StartScopedTask(std::string taskName, uint32_t taskColor)
		{
			return ScopedTask(TaskHandleInfo(this, StartTask(taskName, taskColor)), true);
		}

		using ScopedFrame = RAIIHandle<FrameHandleInfo, CpuProfiler>;

		ScopedFrame StartScopedFrame()
		{
			return ScopedFrame(FrameHandleInfo(this, StartFrame()), true);
		}

	private:
		using hrc = std::chrono::high_resolution_clock;
		size_t frameIndex;
		std::vector<legit::ProfilerTask> profilerTasks;
		hrc::time_point frameStartTime;
		friend struct RAIIHandle<TaskHandleInfo, CpuProfiler>;
	};

	class GpuProfiler
	{
	public:
		GpuProfiler(vk::PhysicalDevice physicalDevice, vk::Device logicalDevice, uint32_t maxTimestampsCount) :
			logicalDevice(logicalDevice),
			timestampQuery(physicalDevice, logicalDevice, maxTimestampsCount)
		{
			frameIndex = 0;
		}

		size_t StartTask(std::string taskName, uint32_t taskColor, vk::PipelineStageFlagBits pipelineStageFlags)
		{
			timestampQuery.AddTimestamp(frameCommandBuffer, profilerTasks.size(), pipelineStageFlags);

			legit::ProfilerTask task;
			task.color = taskColor;
			task.name = taskName;
			task.startTime = -1.0;
			task.endTime = -1.0;
			size_t taskId = profilerTasks.size();
			profilerTasks.push_back(task);


			return taskId;
		}

		void EndTask(size_t taskId)
		{
			assert(profilerTasks.size() == taskId + 1 && profilerTasks.back().endTime < 0.0);
		}

		size_t StartFrame(vk::CommandBuffer commandBuffer)
		{
			this->frameCommandBuffer = commandBuffer;
			profilerTasks.clear();
			timestampQuery.ResetQueryPool(frameCommandBuffer);
			return frameIndex;
		}

		void EndFrame(size_t frameId)
		{
			timestampQuery.AddTimestamp(frameCommandBuffer, profilerTasks.size(), vk::PipelineStageFlagBits::eBottomOfPipe);

			assert(frameId == frameIndex);
			frameIndex++;
		}

		const std::vector<ProfilerTask>& GetProfilerTasks()
		{
			return profilerTasks;
		}

	private:
		struct TaskHandleInfo
		{
			TaskHandleInfo(GpuProfiler* _profiler, size_t _taskId)
			{
				this->profiler = _profiler;
				this->taskId = _taskId;
			}

			void Reset()
			{
				profiler->EndTask(taskId);
			}

			GpuProfiler* profiler;
			size_t taskId;
		};

		struct FrameHandleInfo
		{
			FrameHandleInfo(GpuProfiler* _profiler, size_t _frameId)
			{
				this->profiler = _profiler;
				this->frameId = _frameId;
			}

			void Reset()
			{
				profiler->EndFrame(frameId);
			}

			GpuProfiler* profiler;
			size_t frameId;
		};

	public:
		using ScopedTask = RAIIHandle<TaskHandleInfo, GpuProfiler>;

		ScopedTask StartScopedTask(std::string taskName, uint32_t taskColor, vk::PipelineStageFlagBits pipelineStageFlags)
		{
			return ScopedTask(TaskHandleInfo(this, StartTask(taskName, taskColor, pipelineStageFlags)), true);
		}

		using ScopedFrame = RAIIHandle<FrameHandleInfo, GpuProfiler>;

		ScopedFrame StartScopedFrame(vk::CommandBuffer commandBuffer)
		{
			return ScopedFrame(FrameHandleInfo(this, StartFrame(commandBuffer)), true);
		}

		const std::vector<legit::ProfilerTask>& GetProfilerData()
		{
			return profilerTasks;
		}

		void GatherTimestamps()
		{
			if (profilerTasks.size() > 0)
			{
				legit::TimestampQuery::QueryResult res = timestampQuery.QueryResults(logicalDevice);
				assert(res.size == this->profilerTasks.size() + 1); //1 is because of end-of-frame timestamp

				for (size_t taskIndex = 0; taskIndex < profilerTasks.size(); taskIndex++)
				{
					auto& task = profilerTasks[taskIndex];
					task.startTime = res.data[taskIndex].time;
					task.endTime = res.data[taskIndex + 1].time;
				}
			}
		}

	private:
		vk::Device logicalDevice;
		TimestampQuery timestampQuery;
		size_t frameIndex;
		std::vector<legit::ProfilerTask> profilerTasks;
		vk::CommandBuffer frameCommandBuffer;
		friend struct RAIIHandle<TaskHandleInfo, GpuProfiler>;
	};
}
