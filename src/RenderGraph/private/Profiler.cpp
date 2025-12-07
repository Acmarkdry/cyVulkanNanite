#include "Profiler.h"
#include <cassert>

namespace cyRenderGraph
{
	// CpuProfiler implementations
	CpuProfiler::CpuProfiler()
	{
		frameIndex = 0;
	}

	size_t CpuProfiler::StartTask(std::string taskName, uint32_t taskColor)
	{
		ProfilerTask task;
		task.color = taskColor;
		task.name = taskName;
		task.startTime = GetCurrFrameTimeSeconds();
		task.endTime = -1.0;
		size_t taskId = profilerTasks.size();
		profilerTasks.push_back(task);
		return taskId;
	}

	ProfilerTask CpuProfiler::EndTask(size_t taskId)
	{
		assert(profilerTasks.size() == taskId + 1 && profilerTasks.back().endTime < 0.0);
		profilerTasks.back().endTime = GetCurrFrameTimeSeconds();
		return profilerTasks.back();
	}

	size_t CpuProfiler::StartFrame()
	{
		profilerTasks.clear();
		frameStartTime = hrc::now();
		return frameIndex;
	}

	void CpuProfiler::EndFrame(size_t frameId)
	{
		assert(frameId == frameIndex);
		frameIndex++;
	}

	const std::vector<ProfilerTask>& CpuProfiler::GetProfilerTasks()
	{
		return profilerTasks;
	}

	// GpuProfiler implementations
	GpuProfiler::GpuProfiler(vk::PhysicalDevice physicalDevice, vk::Device logicalDevice, uint32_t maxTimestampsCount) :
		logicalDevice(logicalDevice),
		timestampQuery(physicalDevice, logicalDevice, maxTimestampsCount)
	{
		frameIndex = 0;
	}

	size_t GpuProfiler::StartTask(std::string taskName, uint32_t taskColor, vk::PipelineStageFlagBits pipelineStageFlags)
	{
		timestampQuery.AddTimestamp(frameCommandBuffer, profilerTasks.size(), pipelineStageFlags);

		ProfilerTask task;
		task.color = taskColor;
		task.name = taskName;
		task.startTime = -1.0;
		task.endTime = -1.0;
		size_t taskId = profilerTasks.size();
		profilerTasks.push_back(task);

		return taskId;
	}

	void GpuProfiler::EndTask(size_t taskId)
	{
		assert(profilerTasks.size() == taskId + 1 && profilerTasks.back().endTime < 0.0);
	}

	size_t GpuProfiler::StartFrame(vk::CommandBuffer commandBuffer)
	{
		this->frameCommandBuffer = commandBuffer;
		profilerTasks.clear();
		timestampQuery.ResetQueryPool(frameCommandBuffer);
		return frameIndex;
	}

	void GpuProfiler::EndFrame(size_t frameId)
	{
		timestampQuery.AddTimestamp(frameCommandBuffer, profilerTasks.size(), vk::PipelineStageFlagBits::eBottomOfPipe);

		assert(frameId == frameIndex);
		frameIndex++;
	}

	const std::vector<ProfilerTask>& GpuProfiler::GetProfilerTasks()
	{
		return profilerTasks;
	}

	const std::vector<ProfilerTask>& GpuProfiler::GetProfilerData()
	{
		return profilerTasks;
	}

	void GpuProfiler::GatherTimestamps()
	{
		if (profilerTasks.size() > 0)
		{
			TimestampQuery::QueryResult res = timestampQuery.QueryResults(logicalDevice);
			assert(res.size == this->profilerTasks.size() + 1); //1 is because of end-of-frame timestamp

			for (size_t taskIndex = 0; taskIndex < profilerTasks.size(); taskIndex++)
			{
				auto& task = profilerTasks[taskIndex];
				task.startTime = res.data[taskIndex].time;
				task.endTime = res.data[taskIndex + 1].time;
			}
		}
	}
}
