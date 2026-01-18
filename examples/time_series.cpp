#include <microvtk/microvtk.hpp>
#include <vector>
#include <string>
#include <cmath>
#include <filesystem>

using namespace microvtk;

int main() {
    // 1. Setup PVD Writer for time series
    PvdWriter pvd("wave_simulation.pvd");

    // We will simulate a simple moving wave on a line of points
    const int num_points = 20;
    const int num_steps = 10;
    
    for (int step = 0; step < num_steps; ++step) {
        double time = step * 0.1;
        std::string filename = "wave_" + std::to_string(step) + ".vtu";

        // 2. Setup VTU Writer for this step
        VtuWriter writer(DataFormat::Appended);

        // Generate geometry (Line of points)
        std::vector<double> points;
        points.reserve(num_points * 3);
        
        std::vector<double> elevation; // Scalar data
        elevation.reserve(num_points);

        for (int i = 0; i < num_points; ++i) {
            double x = static_cast<double>(i);
            double z = std::sin(x * 0.5 + time * 5.0); // Moving wave
            
            points.push_back(x);
            points.push_back(0.0);
            points.push_back(z);

            elevation.push_back(z);
        }

        writer.setPoints(points);

        // Define Topology: A series of lines (PolyLine or just Lines)
        // Here we use individual Line cells (2 points per cell)
        std::vector<int32_t> conn;
        std::vector<int32_t> offsets;
        std::vector<uint8_t> types;

        for (int i = 0; i < num_points - 1; ++i) {
            conn.push_back(i);
            conn.push_back(i + 1);
            offsets.push_back(static_cast<int32_t>(conn.size()));
            types.push_back(static_cast<uint8_t>(CellType::Line));
        }

        writer.setCells(conn, offsets, types);

        // Add Point Data
        writer.addPointData("Elevation", elevation);

        // Write the individual VTU file
        writer.write(filename);

        // Add to PVD
        pvd.addStep(time, filename);
    }

    // 3. Save the PVD file (indexes all VTU files)
    // pvd.addStep() automatically updates the file.

    return 0;
}
