import os
import xml.etree.ElementTree as ET
import pytest

def test_pvd_time_series(pvd_files):
    """Verifies PVD file structure and time steps using XML parsing."""

    # Manual XML check for PVD correctness
    tree = ET.parse(pvd_files)
    root = tree.getroot()

    assert root.tag == "VTKFile"
    assert root.attrib["type"] == "Collection"

    collection = root.find("Collection")
    assert collection is not None

    datasets = collection.findall("DataSet")
    assert len(datasets) == 10, f"Expected 10 time steps, found {len(datasets)}"

    # Check first step
    # PvdWriter uses double for time, let's check strict equality or approximate
    assert float(datasets[0].attrib["timestep"]) == 0.0
    assert datasets[0].attrib["file"] == "wave_0.vtu"

    # Verify that the referenced file actually exists
    assert os.path.exists(datasets[0].attrib["file"])
