#pragma once
#include "NaniteInstance.h"
#include "NaniteMesh.h"

namespace Nanite 
{
	class NaniteScene 
	{
	public:
	
		void loadNaniteMesh(vkglTF::Model& model);
		void generateNaniteInfo();
		
		NaniteInstance instance;
	};
}
