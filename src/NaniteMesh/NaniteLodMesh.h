#pragma once
#include <memory>
#include <vulkan/vulkan_core.h>

#include "Cluster.h"
#include "ClusterGroup.h"
#include "Const.h"
#include "NaniteMesh.h"
#include <OpenMesh/Tools/Decimater/DecimaterT.hh>
#include <OpenMesh/Tools/Decimater/ModQuadricT.hh>

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
        int clusterNum;
        NaniteTriMesh mesh;
    	Graph triangleGraph;	
    	std::vector<bool> isEdgeVertices;
    	std::vector<bool> islastLodEdgeVertices;
    	OpenMesh::HPropHandleT<int32_t> clusterGroupIndexPropHandle;
    	std::vector<idx_t> triangleClusterIndex;
    	std::vector<idx_t> clusterGroupIndex;
    	
    	std::vector<uint32_t> triangleIndicesSortedByClusterIdx; // face_idx sort by cluster
    	std::vector<uint32_t> triangleVertexIndicesSortedByClusterIdx;
    	
    	std::vector<Cluster> clusters;
    	
    	int lodLevel;
    	
        // cluster构建
        void buildTriangleGraph();
        void generateCluster();
        
        void getBoundingSphere(Cluster& cluster);
        void calcSurfaceArea(Cluster& cluster);
        
        void createSortedIndexBuffer(VulkanExampleBase& link);
        
        void initUniqueVertexBuffer();
    	
    	vks::Buffer sortedIndices;
    	std::vector<vkglTF::Vertex> uniqueVertexBuffer;

        // mesh lod生成
        std::vector<ClusterGroup> clusterGroups;
        
        void assignTriangleClusterGroup(NaniteLodMesh& lastLoad);
        
        // cluster group构建
        std::vector<ClusterGroup> oldClusterGroups;
    	std::unordered_map<int, int> clusterColorAssignment;
    	Graph clusterGraph;
    	int clusterGroupNum;
    	
    	void generateClusterGroup();
    	
    	void simplifyMesh(NaniteTriMesh& mesh);
    	
    	void buildClusterGraph();
    	void colorClusterGraph();
    	void calcBoundingSphereFromChildren(Cluster& cluster, NaniteLodMesh& lastLOD);
        
        // bvh 构建相关
        void createBVH();
        void buildBVH();
        void updateBVHError();
        void updateBVHErrorCore(std::shared_ptr<NaniteBVHNode> currNode, float& currNodeError, glm::vec4& currNodeBoundingSphere);
        void traverseBVH();
        void getClusterGroupAABB(ClusterGroup & clusterGroup);
        void flattenBVH();

    	// 序列化
    	nlohmann::json toJson();
    	void fromJson(const nlohmann::json& json);
    };
}

namespace OpenMesh {
	namespace Decimater {


		//== CLASS DEFINITION =========================================================


		/** \brief Mesh decimation module computing collapse priority based on error quadrics.
		 *
		 *  This module can be used as a binary and non-binary module.
		 */
		template <class MeshT>
		class MyModQuadricT : public ModBaseT<MeshT>
		{
		public:

			// Defines the types Self, Handle, Base, Mesh, and CollapseInfo
			// and the memberfunction name()
			DECIMATING_MODULE(MyModQuadricT, MeshT, Quadric);

		public:

			/** Constructor
			 *  \internal
			 */
			explicit MyModQuadricT(MeshT& _mesh)
				: Base(_mesh, false)
			{
				unset_max_err();
				Base::mesh().add_property(quadrics_);
			}


			/// Destructor
			virtual ~MyModQuadricT()
			{
				Base::mesh().remove_property(quadrics_);
			}


		public: // inherited

			/// Initalize the module and prepare the mesh for decimation.
			virtual void initialize(void) override;

			/** Compute collapse priority based on error quadrics.
			 *
			 *  \see ModBaseT::collapse_priority() for return values
			 *  \see set_max_err()
			 */
			virtual float collapse_priority(const CollapseInfo& _ci) override
			{
				using namespace OpenMesh;

				typedef Geometry::QuadricT<double> Q;

				Q q = Base::mesh().property(quadrics_, _ci.v0);
				q += Base::mesh().property(quadrics_, _ci.v1);

				double err = q(_ci.p1);

				//min_ = std::min(err, min_);
				//max_ = std::max(err, max_);

				//double err = q( p );

				return float((err < max_err_) ? err : float(Base::ILLEGAL_COLLAPSE));
			}


			/// Post-process halfedge collapse (accumulate quadrics)
			virtual void postprocess_collapse(const CollapseInfo& _ci) override
			{
				total_err_ += collapse_priority(_ci);
				Base::mesh().property(quadrics_, _ci.v1) +=
					Base::mesh().property(quadrics_, _ci.v0);
			}

			/// set the percentage of maximum quadric error
			void set_error_tolerance_factor(double _factor) override;



		public: // specific methods

			/** Set maximum quadric error constraint and enable binary mode.
			 *  \param _err    Maximum error allowed
			 *  \param _binary Let the module work in non-binary mode in spite of the
			 *                 enabled constraint.
			 *  \see unset_max_err()
			 */
			void set_max_err(double _err, bool _binary = true)
			{
				max_err_ = _err;
				Base::set_binary(_binary);
			}

			/// Unset maximum quadric error constraint and restore non-binary mode.
			/// \see set_max_err()
			void unset_max_err(void)
			{
				max_err_ = DBL_MAX;
				Base::set_binary(false);
			}

			/// Return value of max. allowed error.
			double max_err() const { return max_err_; }

			double total_err() const { return total_err_; }

			void clear_total_err() { total_err_ = 0.0f; }
		private:
			double total_err_ = 0.0f;

			// maximum quadric error
			double max_err_;

			// this vertex property stores a quadric for each vertex
			VPropHandleT< Geometry::QuadricT<double> >  quadrics_;
		};

		//=============================================================================
	} // END_NS_DECIMATER
} // END_NS_OPENMESH
//=============================================================================


//== NAMESPACE ===============================================================

namespace OpenMesh { // BEGIN_NS_OPENMESH
	namespace Decimater { // BEGIN_NS_DECIMATER


		//== IMPLEMENTATION ==========================================================


		template<class DecimaterType>
		void
			MyModQuadricT<DecimaterType>::
			initialize()
		{
			using Geometry::Quadricd;
			// alloc quadrics
			if (!quadrics_.is_valid())
				Base::mesh().add_property(quadrics_);

			// clear quadrics
			typename Mesh::VertexIter  v_it = Base::mesh().vertices_begin(),
				v_end = Base::mesh().vertices_end();

			for (; v_it != v_end; ++v_it)
				Base::mesh().property(quadrics_, *v_it).clear();

			// calc (normal weighted) quadric
			typename Mesh::FaceIter          f_it = Base::mesh().faces_begin(),
				f_end = Base::mesh().faces_end();

			typename Mesh::FaceVertexIter    fv_it;
			typename Mesh::VertexHandle      vh0, vh1, vh2;
			typedef Vec3d                    Vec3;

			for (; f_it != f_end; ++f_it)
			{
				fv_it = Base::mesh().fv_iter(*f_it);
				vh0 = *fv_it;  ++fv_it;
				vh1 = *fv_it;  ++fv_it;
				vh2 = *fv_it;

				Vec3 v0, v1, v2;
				{
					using namespace OpenMesh;

					v0 = vector_cast<Vec3>(Base::mesh().point(vh0));
					v1 = vector_cast<Vec3>(Base::mesh().point(vh1));
					v2 = vector_cast<Vec3>(Base::mesh().point(vh2));
				}

				Vec3 n = (v1 - v0) % (v2 - v0);
				double area = n.norm();
				if (area > FLT_MIN)
				{
					n /= area;
					area *= 0.5;
				}

				const double a = n[0];
				const double b = n[1];
				const double c = n[2];
				const double d = -(vector_cast<Vec3>(Base::mesh().point(vh0)) | n);

				Quadricd q(a, b, c, d);
				q *= area;

				Base::mesh().property(quadrics_, vh0) += q;
				Base::mesh().property(quadrics_, vh1) += q;
				Base::mesh().property(quadrics_, vh2) += q;
			}
		}

		//-----------------------------------------------------------------------------

		template<class MeshT>
		void MyModQuadricT<MeshT>::set_error_tolerance_factor(double _factor) {
			if (this->is_binary()) {
				if (_factor >= 0.0 && _factor <= 1.0) {
					// the smaller the factor, the smaller max_err_ gets
					// thus creating a stricter constraint
					// division by error_tolerance_factor_ is for normalization
					double max_err = max_err_ * _factor / this->error_tolerance_factor_;
					set_max_err(max_err);
					this->error_tolerance_factor_ = _factor;

					initialize();
				}
			}
		}

		//=============================================================================
	} // END_NS_DECIMATER
} // END_NS_OPENMESH
//=============================================================================
