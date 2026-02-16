#pragma once
#include <vector>
#include <map>
#include <string>
#include <iostream>
#include <fstream>
#include <functional>
#include <memory>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <vulkan/vulkan.hpp>
#include <json.hpp>

#include "Mesh.h"
#include "PresentQueue.h"
#include "RenderGraphLink.h"
#include "VertexDeclaration.h"

struct Object
{
	Object()
	{
		mesh = nullptr;
		objToWorld = glm::mat4(1.0f);
		albedoColor = glm::vec3(1.0f, 1.0f, 1.0f);
		emissiveColor = glm::vec3(1.0f, 1.0f, 1.0f);
		isShadowReceiver = true;
	}

	Mesh* mesh;
	glm::mat4 objToWorld;

	glm::vec3 albedoColor;
	glm::vec3 emissiveColor;
	bool isShadowReceiver;
};

struct Camera
{
	Camera()
	{
		pos = glm::vec3(0.0f);
		vertAngle = 0.0f;
		horAngle = 0.0f;
	}

	glm::vec3 pos;
	float vertAngle, horAngle;

	glm::mat4 GetTransformMatrix() const
	{
		return glm::translate(pos) * glm::rotate(horAngle, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::rotate(vertAngle, glm::vec3(1.0f, 0.0f, 0.0f));
	}
};

inline glm::vec3 ReadJsonVec3f(const nlohmann::json& node)
{
	return glm::vec3(node[0].get<float>(), node[1].get<float>(), node[2].get<float>());
}

class Scene
{
public:
	enum struct GeometryTypes
	{
		Triangles,
		RegularPoints,
		SizedPoints
	};

	Scene(const nlohmann::json& sceneConfig, cyRenderGraph::Core* core, GeometryTypes geometryType)
	{
		this->core = core;

		vertexDecl = Mesh::GetVertexDeclaration();

		/* TODO: 启用 tinyobj 加载时取消注释
		cyRenderGraph::ExecuteOnceQueue transferQueue(core);

		std::map<std::string, Mesh*> nameToMesh;

		auto transferCommandBuffer = transferQueue.BeginCommandBuffer();
		{
			const auto& meshArray = sceneConfig["meshes"];
			for (size_t meshIndex = 0; meshIndex < meshArray.size(); meshIndex++)
			{
				const auto& currMeshNode = meshArray[meshIndex];

				std::string meshFilename = currMeshNode.value("filename", "<unspecified>");
				glm::vec3 scale = ReadJsonVec3f(currMeshNode["scale"]);

				auto meshData = MeshData(meshFilename, scale);
				switch (geometryType)
				{
				case GeometryTypes::RegularPoints:
					{
						float splatSize = 0.1f;
						meshData = MeshData::GeneratePointMeshRegular(meshData, std::pow(1.0f / splatSize, 2.0f));
					}
					break;
				case GeometryTypes::SizedPoints:
					{
						meshData = MeshData::GeneratePointMeshSized(meshData, 1);
					}
					break;
				default:
					break;
				}
				auto mesh = std::make_unique<Mesh>(meshData, core->GetPhysicalDevice(), core->GetLogicalDevice(), transferCommandBuffer);
				meshes.push_back(std::move(mesh));

				std::string meshName = currMeshNode.value("name", "<unspecified>");
				nameToMesh[meshName] = meshes.back().get();
			}
		}
		transferQueue.EndCommandBuffer();

		const auto& objectsArray = sceneConfig["objects"];
		for (size_t objectIndex = 0; objectIndex < objectsArray.size(); objectIndex++)
		{
			Object object;

			const auto& currObjectNode = objectsArray[objectIndex];
			std::string meshName = currObjectNode.value("mesh", "<unspecified>");
			if (!nameToMesh.contains(meshName))
			{
				std::cout << "Mesh " << meshName << " not specified";
				continue;
			}

			object.mesh = nameToMesh[meshName];
			glm::vec3 rotationVec = ReadJsonVec3f(currObjectNode["angle"]);
			object.objToWorld = glm::translate(ReadJsonVec3f(currObjectNode["pos"]));
			if (glm::length(rotationVec) > 1e-3f)
				object.objToWorld = object.objToWorld * glm::rotate(glm::length(rotationVec), glm::normalize(rotationVec));

			object.albedoColor = ReadJsonVec3f(currObjectNode["albedoColor"]);
			object.emissiveColor = ReadJsonVec3f(currObjectNode["emissiveColor"]);
			object.isShadowReceiver = currObjectNode.value("isShadowCaster", true);

			if (currObjectNode.value("isMarker", false))
			{
				markerObjectIndex = objects.size();
			}

			objects.push_back(object);
		}
		*/
	}

	using ObjectCallback = std::function<void(glm::mat4 objectToWorld, glm::vec3 albedoColor, glm::vec3 emissiveColor, vk::Buffer vertexBuffer, vk::Buffer indexBuffer, uint32_t verticesCount, uint32_t indicesCount)>;

	void IterateObjects(ObjectCallback objectCallback)
	{
		for (auto& object : objects)
		{
			objectCallback(object.objToWorld, object.albedoColor, object.emissiveColor, object.mesh->vertexBuffer->GetBuffer(), object.mesh->indexBuffer ? object.mesh->indexBuffer->GetBuffer() : nullptr, static_cast<uint32_t>(object.mesh->verticesCount), static_cast<uint32_t>(object.mesh->indicesCount));
		}
	}

private:
	std::vector<std::unique_ptr<Mesh>> meshes;
	std::vector<Object> objects;
	size_t markerObjectIndex;

	cyRenderGraph::VertexDeclaration vertexDecl;
	cyRenderGraph::Core* core;
};
