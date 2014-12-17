#include "stdafx.h"
#include "SphericalTerrainPatch.h"

#include "BlockData.h"
#include "Camera.h"
#include "Chunk.h"
#include "FloraGenerator.h"
#include "GameManager.h"
#include "MessageManager.h"
#include "Options.h"
#include "WorldStructs.h"
#include "GpuMemory.h"
#include "RenderUtils.h"
#include "VoxelPlanetMapper.h"
#include "MeshManager.h"
#include "RPC.h"

#include "utils.h"

#define INDICES_PER_QUAD 6
const int INDICES_PER_PATCH = (PATCH_WIDTH - 1) * (PATCH_WIDTH - 1) * INDICES_PER_QUAD;


SphericalTerrainPatch::~SphericalTerrainPatch() {
    destroy();
}

void SphericalTerrainPatch::init(const f64v2& gridPosition,
                                 CubeFace cubeFace,
                                 const SphericalTerrainData* sphericalTerrainData,
                                 f64 width,
                                 TerrainRpcDispatcher* dispatcher) {
    m_gridPosition = gridPosition;
    m_cubeFace = cubeFace;
    m_sphericalTerrainData = sphericalTerrainData;
    m_width = width;
    m_dispatcher = dispatcher;

    // Approximate the world position for now //TODO(Ben): Better
    f64v2 centerGridPos = gridPosition + f64v2(width);

    const i32v3& coordMapping = CubeCoordinateMappings[(int)m_cubeFace];
    const f32v3& coordMults = CubeCoordinateMults[(int)m_cubeFace];

    m_worldPosition[coordMapping.x] = centerGridPos.x;
    m_worldPosition[coordMapping.y] = sphericalTerrainData->getRadius() * coordMults.y;
    m_worldPosition[coordMapping.z] = centerGridPos.y;

    m_worldPosition = glm::normalize(m_worldPosition);
}

void SphericalTerrainPatch::update(const f64v3& cameraPos) {
    // Calculate distance from camera
    m_distance = glm::length(m_worldPosition - cameraPos);
    
    if (m_children) {
   
        if (m_distance > m_width * 4.1) {
            // Out of range, kill children
            delete[] m_children;
            m_children = nullptr;
        } else if (hasMesh()) {
            // In range, but we need to remove our mesh.
            // Check to see if all children are renderable
            bool deleteMesh = true;
            for (int i = 0; i < 4; i++) {
                if (!m_children[i].isRenderable()) {
                    deleteMesh = true;
                    break;
                }
            }
            
            if (deleteMesh) {
                // Children are renderable, free mesh
                destroyMesh();
            }
        }
    } else if (m_distance < m_width * 4.0) {
        m_children = new SphericalTerrainPatch[4];
        // Segment into 4 children
        for (int z = 0; z < 2; z++) {
            for (int x = 0; x < 2; x++) {
                m_children[(z << 1) + x].init(m_gridPosition + f64v2(m_width / 2.0 * x, m_width / 2.0 * z),
                                       m_cubeFace, m_sphericalTerrainData, m_width / 2.0,
                                       m_meshManager);
            }
        }
    }
    
    // Recursively update children if they exist
    if (m_children) {
        for (int i = 0; i < 4; i++) {
            m_children[i].update(cameraPos);
        }
    }
}

void SphericalTerrainPatch::destroy() {
    destroyMesh();
    delete[] m_children;
    m_children = nullptr;
}

void SphericalTerrainPatch::draw(const f64v3& cameraPos, const f32m4& VP, vg::GLProgram* program) {
    if (m_children) {
        // Draw the children
        for (int i = 0; i < 4; i++) {
            m_children[i].draw(cameraPos, VP, program);
        }
        // If we have a mesh, we should still render it for now
        if (!hasMesh()) return;
    }

    // If we don't have a mesh, generate one
    if (!(hasMesh())) {
        update(cameraPos);
        float heightData[PATCH_WIDTH][PATCH_WIDTH];
        memset(heightData, 0, sizeof(heightData));
        generateMesh(heightData);
    }
  
    // Set up matrix
    f32m4 matrix(1.0);
    setMatrixTranslation(matrix, -cameraPos);
    matrix = VP * matrix;


    glUniformMatrix4fv(program->getUniform("unWVP"), 1, GL_FALSE, &matrix[0][0]);
    
    vg::GpuMemory::bindBuffer(m_vbo, vg::BufferTarget::ARRAY_BUFFER);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                          sizeof(TerrainVertex),
                          offsetptr(TerrainVertex, position));
    glVertexAttribPointer(1, 3, GL_UNSIGNED_BYTE, GL_TRUE,
                          sizeof(TerrainVertex),
                          offsetptr(TerrainVertex, color));
    
    vg::GpuMemory::bindBuffer(m_ibo, vg::BufferTarget::ELEMENT_ARRAY_BUFFER);
    glDrawElements(GL_TRIANGLES, INDICES_PER_PATCH, GL_UNSIGNED_SHORT, 0);
}

bool SphericalTerrainPatch::isRenderable() const {
    if (hasMesh()) return true;
    if (m_children) {
        for (int i = 0; i < 4; i++) {
            if (!m_children[i].isRenderable()) return false;
        }
        return true;
    }
    return false;
}

void SphericalTerrainPatch::destroyMesh() {
    if (m_vbo) {
        vg::GpuMemory::freeBuffer(m_vbo);
        m_vbo = 0;
    }
    if (m_ibo) {
        vg::GpuMemory::freeBuffer(m_ibo);
        m_ibo = 0;
    }
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
}