#pragma once
#include <map>
#include <vector>
#include <iostream>
#include <string>
#include <memory>

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

#include "StagedResource.h"
#include "VertexDeclaration.h"

#pragma pack(push, 1)
struct PointMeshVertex
{
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec2 uv;
};
#pragma pack(pop)

inline cyRenderGraph::VertexDeclaration GetPointMeshVertexDeclaration()
{
	cyRenderGraph::VertexDeclaration vertexDecl;
	vertexDecl.AddVertexInputBinding(0, sizeof(PointMeshVertex));
	vertexDecl.AddVertexAttribute(0, offsetof(PointMeshVertex, pos), cyRenderGraph::VertexDeclaration::AttribTypes::vec3, 0);
	vertexDecl.AddVertexAttribute(0, offsetof(PointMeshVertex, normal), cyRenderGraph::VertexDeclaration::AttribTypes::vec3, 1);
	vertexDecl.AddVertexAttribute(0, offsetof(PointMeshVertex, uv), cyRenderGraph::VertexDeclaration::AttribTypes::vec2, 2);

	return vertexDecl;
}

/* TODO: 启用 tinyobj 加载时取消注释
namespace tinyobj
{
	inline bool operator <(const tinyobj::index_t& left, const tinyobj::index_t& right)
	{
		return std::tie(left.vertex_index, left.normal_index, left.texcoord_index) < std::tie(right.vertex_index, right.normal_index, right.texcoord_index);
	}
}
*/

struct PointMesh
{
	/* TODO: 启用 tinyobj 加载时取消注释
	PointMesh(std::string filename, vk::PhysicalDevice physicalDevice, vk::Device logicalDevice, vk::CommandBuffer transferCommandBuffer, glm::vec3 scale)
	{
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;

		std::string warn;
		std::string err;
		std::cout << "Loading mesh: " << filename << "\n";
		bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename.c_str(), nullptr, true);
		std::cout << "Warnings: " << warn << "\n";
		if (!ret)
		{
			std::cout << "Errors: " << err << "\n";
		}
		else
		{
			std::cout << "Mesh loaded\n";
		}

		using IndexType = uint32_t;
		std::vector<PointMeshVertex> vertices;
		std::vector<IndexType> indices;
		std::map<tinyobj::index_t, size_t> deduplicatedIndices;
		for (auto& shape : shapes)
		{
			for (auto& index : shape.mesh.indices)
			{
				if (deduplicatedIndices.find(index) == deduplicatedIndices.end())
				{
					deduplicatedIndices[index] = vertices.size();
					PointMeshVertex vertex;
					vertex.pos = glm::vec3(attrib.vertices[index.vertex_index * 3 + 0], attrib.vertices[index.vertex_index * 3 + 1], attrib.vertices[index.vertex_index * 3 + 2]) * scale;
					if (index.normal_index != -1) vertex.normal = glm::vec3(attrib.normals[index.normal_index * 3 + 0], attrib.normals[index.normal_index * 3 + 1], attrib.normals[index.normal_index * 3 + 2]);
					else vertex.normal = glm::vec3(1.0f, 0.0f, 0.0f);
					if (index.texcoord_index != -1) vertex.uv = glm::vec2(attrib.texcoords[index.texcoord_index * 2 + 0], attrib.texcoords[index.texcoord_index * 2 + 1]);
					else vertex.uv = glm::vec2(0.0f, 0.0f);
					vertices.push_back(vertex);
				}
				indices.push_back(IndexType(deduplicatedIndices[index]));
			}
		}
		indicesCount = indices.size();

		vertexBuffer = std::make_unique<cyRenderGraph::StagedBuffer>(physicalDevice, logicalDevice, vertices.size() * sizeof(PointMeshVertex), vk::BufferUsageFlagBits::eVertexBuffer);
		indexBuffer = std::make_unique<cyRenderGraph::StagedBuffer>(physicalDevice, logicalDevice, indices.size() * sizeof(IndexType), vk::BufferUsageFlagBits::eIndexBuffer);

		memcpy(vertexBuffer->Map(), vertices.data(), sizeof(PointMeshVertex) * vertices.size());
		vertexBuffer->Unmap(transferCommandBuffer);

		memcpy(indexBuffer->Map(), indices.data(), sizeof(IndexType) * indices.size());
		indexBuffer->Unmap(transferCommandBuffer);
	}
	*/

	std::unique_ptr<cyRenderGraph::StagedBuffer> vertexBuffer;
	std::unique_ptr<cyRenderGraph::StagedBuffer> indexBuffer;
	size_t indicesCount;
};
