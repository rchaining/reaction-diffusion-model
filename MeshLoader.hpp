#pragma once
#include <Metal/Metal.hpp>
#include <iostream>
#include <string>
#include <vector>

// Define the implementation in ONE cpp file (we'll do this in Renderer.cpp)
// #define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

struct Vertex {
  float position[4];
  float normal[4];
  float color[4];
};

class MeshLoader {
public:
  static std::vector<Vertex> loadObj(const std::string &filename) {
    tinyobj::ObjReaderConfig reader_config;
    reader_config.mtl_search_path = "./"; // Path to material files

    tinyobj::ObjReader reader;

    if (!reader.ParseFromFile(filename, reader_config)) {
      if (!reader.Error().empty()) {
        std::cerr << "TinyObjReader: " << reader.Error();
      }
      return {};
    }

    if (!reader.Warning().empty()) {
      std::cout << "TinyObjReader: " << reader.Warning();
    }
    auto &attrib = reader.GetAttrib();
    auto &shapes = reader.GetShapes();
    std::vector<Vertex> vertices;

    // Loop over shapes
    for (size_t s = 0; s < shapes.size(); s++) {
      // Loop over faces(polygon)
      size_t index_offset = 0;
      for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
        size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);

        // Loop over vertices in the face.
        for (size_t v = 0; v < fv; v++) {
          // access to vertex
          tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
          Vertex vertex;

          // Position
          tinyobj::real_t vx = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
          tinyobj::real_t vy = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
          tinyobj::real_t vz = attrib.vertices[3 * size_t(idx.vertex_index) + 2];
          // Scale (Should move to an external matrix at some point)
          vertex.position[0] = vx * 0.5f;
          vertex.position[1] = vy * 0.5f;
          vertex.position[2] = vz * 0.5f;
          vertex.position[3] = 1.0f;
          // Normal (if they exist)
          if (idx.normal_index >= 0) {
            tinyobj::real_t nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
            tinyobj::real_t ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
            tinyobj::real_t nz = attrib.normals[3 * size_t(idx.normal_index) + 2];
            vertex.normal[0] = nx;
            vertex.normal[1] = ny;
            vertex.normal[2] = nz;
            vertex.normal[3] = 0.0f; // Vector not point, so w=0
          } else {
            //fallback if no normals
            vertex.normal[2] = 1.0f;
          }

          // Color (Just our default for now)
          vertex.color[0] = 1.0f;
          vertex.color[1] = 1.0f;
          vertex.color[2] = 1.0f;
          vertex.color[3] = 1.0f;
          
          // Save vert
          vertices.push_back(vertex);
        }
        index_offset += fv;
      }
    }
    std::cout << "Loaded " << vertices.size() << " vertices." << std::endl;
    return vertices;
  }
};