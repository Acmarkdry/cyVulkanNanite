#include "PBRTextureBuffer.h"

void vks::Textures::destroy()
{
	environmentCube.destroy();
	irradianceCube.destroy();
	prefilteredCube.destroy();
	lutBrdf.destroy();
	albedoMap.destroy();
	normalMap.destroy();
	aoMap.destroy();
	metallicMap.destroy();
	roughnessMap.destroy();
	hizBuffer.destroy();
}

void vks::UniformBuffers::destroy()
{
	scene.destroy();
	skybox.destroy();
	params.destroy();
}
