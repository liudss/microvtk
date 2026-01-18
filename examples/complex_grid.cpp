#include <microvtk/microvtk.hpp>
#include <vector>
#include <array>

using namespace microvtk;

int main() {
    // 1. Setup Writer
    VtuWriter writer(DataFormat::Appended);

    // 2. Define Points
    // We will build a Hexahedron (cube) and a Tetrahedron next to it.
    // Hex points: 0-7
    // Tetra points: 8-11
    std::vector<double> points = {
        // Hexahedron (0,0,0) to (1,1,1)
        0,0,0,  1,0,0,  1,1,0,  0,1,0, // Bottom face
        0,0,1,  1,0,1,  1,1,1,  0,1,1, // Top face
        
        // Tetrahedron (sitting on top of the cube roughly)
        0.5, 0.5, 1.0, // Base 1 (Shared geometry conceptually, but new point index for simplicity)
        1.5, 0.5, 1.0, // Base 2
        0.5, 1.5, 1.0, // Base 3
        0.5, 0.5, 2.0  // Apex
    };
    writer.setPoints(points);

    // 3. Define Topology
    std::vector<int32_t> conn;
    std::vector<int32_t> offsets;
    std::vector<uint8_t> types;

    // Cell 0: Hexahedron
    // VTK Hex ordering: bottom face ccw, then top face ccw
    std::array<int32_t, 8> hex_ids = {0, 1, 2, 3, 4, 5, 6, 7};
    for(auto id : hex_ids) conn.push_back(id);
    offsets.push_back(static_cast<int32_t>(conn.size()));
    types.push_back(static_cast<uint8_t>(CellType::Hexahedron));

    // Cell 1: Tetrahedron
    std::array<int32_t, 4> tetra_ids = {8, 9, 10, 11};
    for(auto id : tetra_ids) conn.push_back(id);
    offsets.push_back(static_cast<int32_t>(conn.size()));
    types.push_back(static_cast<uint8_t>(CellType::Tetra));

    writer.setCells(conn, offsets, types);

    // 4. Point Data
    // "Temperature": Gradient along Z
    std::vector<double> temperature;
    for (size_t i = 0; i < points.size() / 3; ++i) {
        double z = points[i * 3 + 2];
        temperature.push_back(z * 100.0);
    }
    writer.addPointData("Temperature", temperature);

    // 5. Cell Data
    // "MaterialID": 1 for Hex, 2 for Tetra
    std::vector<int> material_id = {1, 2};
    writer.addCellData("MaterialID", material_id);

    // 6. Write
    writer.write("complex_grid.vtu");

    return 0;
}
