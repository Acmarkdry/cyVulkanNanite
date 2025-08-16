#pragma once
#include <memory>
#include <vulkan/vulkan_core.h>

#include "Cluster.h"
#include "ClusterGroup.h"
#include "Const.h"
#include <OpenMesh/Tools/Decimater/DecimaterT.hh>
#include <OpenMesh/Tools/Decimater/ModQuadricT.hh>
#include "VulkanglTFModel.h"

class VulkanExampleBase;

namespace vks
{
	struct VulkanDevice;
}

namespace Nanite
{
	class Cluster;
	class ClusterGroup;
	class NaniteBVHNode;

	class NaniteLodMesh
	{
	public:
		std::vector<ClusterGroup> oldClusterGroups;
		
		NaniteTriMesh mesh;
		OpenMesh::HPropHandleT<int32_t> clusterGroupIndexPropHandle;
		glm::mat4 modelMatrix{1.0f};

		std::vector<uint32_t> triangleIndicesSortedByClusterIdx;
		std::vector<uint32_t> triangleVertexIndicesSortedByClusterIdx;

		uint32_t lodLevel = 0;
		Graph triangleGraph;
		int clusterNum = 0;
		const int targetClusterSize = CLUSTER_SIZE;
		std::vector<idx_t> triangleClusterIndex;
		std::unordered_map<int, int> clusterColorAssignment;
		std::vector<Cluster> clusters;

		Graph clusterGraph;
		int clusterGroupNum = 0;
		const int targetClusterGroupSize = CLUSTER_GROUP_SIZE;
		std::vector<idx_t> clusterGroupIndex;
		std::unordered_map<int, int> clusterGroupColorAssignment;
		std::vector<ClusterGroup> clusterGroups;

		std::vector<bool> isEdgeVertices;
		std::vector<bool> isLastLODEdgeVertices;

		vks::VulkanDevice* device = nullptr;
		const vkglTF::Model* model = nullptr;
		vkglTF::Model::Vertices vertices;
		std::vector<uint32_t> indexBuffer;
		std::vector<vkglTF::Vertex> vertexBuffer;
		std::vector<vkglTF::Vertex> uniqueVertexBuffer;
		std::vector<vkglTF::Primitive> primitives;

		const std::array<glm::vec3, 8> nodeColors = {{
			glm::vec3(1.0f, 0.0f, 0.0f), // red
			glm::vec3(0.0f, 1.0f, 0.0f), // green
			glm::vec3(0.0f, 0.0f, 1.0f), // blue
			glm::vec3(1.0f, 1.0f, 0.0f), // yellow
			glm::vec3(1.0f, 0.0f, 1.0f), // purple
			glm::vec3(0.0f, 1.0f, 1.0f), // cyan
			glm::vec3(1.0f, 0.5f, 0.0f), // orange
			glm::vec3(0.5f, 1.0f, 0.0f), // lime
		}};

		void assignTriangleClusterGroup(NaniteLodMesh& lastLOD);
		void buildTriangleGraph();
		void generateCluster();

		void buildClusterGraph();
		void generateClusterGroup();

		void colorClusterGraph();

		void simplifyMesh(NaniteTriMesh& mymesh);

		void getBoundingSphere(Cluster& cluster);
		void calcBoundingSphereFromChildren(Cluster& cluster, NaniteLodMesh& lastLOD);
		void calcSurfaceArea(Cluster& cluster);

		[[nodiscard]] nlohmann::json toJson();
		void fromJson(const nlohmann::json& j);

		void initVertexBuffer();
		void initUniqueVertexBuffer();
		void createVertexBuffer(VulkanExampleBase& variableLink);
		std::vector<glm::vec3> positions;

	private:
		static constexpr idx_t METIS_RANDOM_SEED = 42;
		static constexpr double SIMPLIFY_PERCENTAGE = 0.5;

		void sortTrianglesByCluster();
		void buildTriangleVertexIndices();
		void initClustersFromFaces();
		
		[[nodiscard]] glm::vec3 pointToVec3(const NaniteTriMesh::Point& p) const;
		
		struct FaceVertices {
			NaniteTriMesh::VertexHandle v0, v1, v2;
		};
		[[nodiscard]] FaceVertices getFaceVertices(NaniteTriMesh::FaceHandle fh) const;
	};
}

namespace OpenMesh::Decimater
{
	template <class MeshT>
	class MyModQuadricT : public ModBaseT<MeshT>
	{
	public:
		DECIMATING_MODULE(MyModQuadricT, MeshT, Quadric);

		explicit MyModQuadricT(MeshT& _mesh)
			: Base(_mesh, false)
		{
			unset_max_err();
			Base::mesh().add_property(quadrics_);
		}

		~MyModQuadricT() override
		{
			Base::mesh().remove_property(quadrics_);
		}

		void initialize() override;

		float collapse_priority(const CollapseInfo& _ci) override
		{
			using Q = Geometry::QuadricT<double>;

			Q q = Base::mesh().property(quadrics_, _ci.v0);
			q += Base::mesh().property(quadrics_, _ci.v1);

			const double err = q(_ci.p1);
			return static_cast<float>((err < max_err_) ? err : Base::ILLEGAL_COLLAPSE);
		}

		void postprocess_collapse(const CollapseInfo& _ci) override
		{
			total_err_ += collapse_priority(_ci);
			Base::mesh().property(quadrics_, _ci.v1) +=
				Base::mesh().property(quadrics_, _ci.v0);
		}

		void set_error_tolerance_factor(double _factor) override;

		void set_max_err(double _err, bool _binary = true)
		{
			max_err_ = _err;
			Base::set_binary(_binary);
		}

		void unset_max_err()
		{
			max_err_ = DBL_MAX;
			Base::set_binary(false);
		}

		[[nodiscard]] double max_err() const noexcept { return max_err_; }
		[[nodiscard]] double total_err() const noexcept { return total_err_; }
		void clear_total_err() noexcept { total_err_ = 0.0; }

	private:
		double total_err_ = 0.0;
		double max_err_ = DBL_MAX;
		VPropHandleT<Geometry::QuadricT<double>> quadrics_;
	};
}

namespace OpenMesh::Decimater
{
	template <class DecimaterType>
	void MyModQuadricT<DecimaterType>::initialize()
	{
		using Geometry::Quadricd;
		
		if (!quadrics_.is_valid())
			Base::mesh().add_property(quadrics_);

		// Clear quadrics
		for (auto v_it = Base::mesh().vertices_begin(); v_it != Base::mesh().vertices_end(); ++v_it)
			Base::mesh().property(quadrics_, *v_it).clear();

		// Calculate (normal weighted) quadric
		for (auto f_it = Base::mesh().faces_begin(); f_it != Base::mesh().faces_end(); ++f_it)
		{
			auto fv_it = Base::mesh().fv_iter(*f_it);
			const auto vh0 = *fv_it++;
			const auto vh1 = *fv_it++;
			const auto vh2 = *fv_it;

			const Vec3d v0 = vector_cast<Vec3d>(Base::mesh().point(vh0));
			const Vec3d v1 = vector_cast<Vec3d>(Base::mesh().point(vh1));
			const Vec3d v2 = vector_cast<Vec3d>(Base::mesh().point(vh2));

			Vec3d n = (v1 - v0) % (v2 - v0);
			double area = n.norm();
			
			if (area > FLT_MIN)
			{
				n /= area;
				area *= 0.5;
			}

			const double d = -(vector_cast<Vec3d>(Base::mesh().point(vh0)) | n);
			Quadricd q(n[0], n[1], n[2], d);
			q *= area;

			Base::mesh().property(quadrics_, vh0) += q;
			Base::mesh().property(quadrics_, vh1) += q;
			Base::mesh().property(quadrics_, vh2) += q;
		}
	}

	template <class MeshT>
	void MyModQuadricT<MeshT>::set_error_tolerance_factor(double _factor)
	{
		if (!this->is_binary())
			return;
			
		if (_factor >= 0.0 && _factor <= 1.0)
		{
			const double max_err = max_err_ * _factor / this->error_tolerance_factor_;
			set_max_err(max_err);
			this->error_tolerance_factor_ = _factor;
			initialize();
		}
	}
}