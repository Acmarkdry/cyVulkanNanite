#pragma once
// All PassContext, RenderPassDesc, ComputePassDesc, TransferPassDesc,
// ImagePresentPassDesc, FrameSyncBeginPassDesc, FrameSyncEndPassDesc, etc.
// have been merged into RenderGraph.h to break the circular include dependency.
// This file is kept as a compatibility forwarding header.
#include "RenderGraph.h"
