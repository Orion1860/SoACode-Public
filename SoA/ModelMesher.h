#pragma once
#ifndef ModelMesher_h__
#define ModelMesher_h__

class VoxelMatrix;
class VoxelModel;
class VoxelModelMesh;
class VoxelModelVertex;

class ModelMesher {
public:
    static VoxelModelMesh createMesh(const VoxelModel* model);
    static VoxelModelMesh createMarchingCubesMesh(const VoxelModel* model);

private:
    // *** Regular ***
    static void genMatrixMesh(const VoxelMatrix& matrix, std::vector<VoxelModelVertex>& vertices, std::vector<ui32>& indices);

    // *** Marching Cubes ***
    static color3 getColor(const f32v3& pos, const VoxelMatrix& matrix);
    static void marchingCubesCross(const VoxelMatrix& matrix,
                                   float minValue, f32v4 * points,
                                   std::vector<VoxelModelVertex>& vertices);
    static f32 getMarchingPotential(const VoxelMatrix& matrix, int x, int y, int z);
};

#endif //ModelMesher_h__