#pragma once
#include <unordered_map>
#include <OpenMesh/Core/Mesh/TriMesh_ArrayKernelT.hh>

namespace Nanite
{
    class Graph
    {
    public:
        std::vector<uint32_t, std::unordered_map<int, uint32_t>> adjMap;
        
        
    };

    // 启用normal和texcoord2d
    struct NaniteOpenMeshTraits : public OpenMesh::DefaultTraits
    {
        VertexAttributes(OpenMesh::Attributes::Normal | OpenMesh::Attributes::TexCoord2D);
    };

    typedef OpenMesh::TriMesh_ArrayKernelT<NaniteOpenMeshTraits> NaniteTriMesh;

    void TestNaniteTriMesh()
    {
        NaniteTriMesh mesh;
        NaniteTriMesh::VertexHandle vh1 = mesh.add_vertex(NaniteTriMesh::Point(0,0,0));

        mesh.set_normal(vh1, NaniteTriMesh::Normal(0,0,1));
        mesh.set_texcoord2D(vh1, NaniteTriMesh::TexCoord2D(0,0));
        std::vector<NaniteTriMesh::VertexHandle> face_handles;
        face_handles.emplace_back(vh1);
        face_handles.emplace_back(vh1);
        face_handles.emplace_back(vh1);

        mesh.add_face(face_handles);
    }

    // 会传入给shader
    class ClusterInfo
    {
        public:
    };

    class ErrorInfo
    {
        public:
    };

}
