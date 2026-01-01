# File to run model setup, mesh generation, compute file writing

from mesh import *
from write_compute_file import *

if __name__ == "__main__":

    # define general information
    solver = "GAUSS_SEIDEL"
    tolerance = 0.002
    maxiter = 400
    stages = 1
    solver_info = [solver, tolerance, maxiter]
    stage_info = [stages]
    assumption = "plane_stress"
    thickness = 0.1
    assumption_info = [assumption, thickness]

    # define domain size and element size
    domain_width = 10
    domain_height = 10
    element_size = 3

    # generate the mesh
    coordinates, node_numbers, elements = generate_quad_mesh(
        domain_width, domain_height, element_size
    )

    # define material properties
    materials = [
        {
            "name": "material_1",
            "formulation": "LinearElastic",
            "E": 20000,  # kPa
            "nu": 0.2,
        },
        {
            "name": "material_2",
            "formulation": "Plastic",
            "E": 30000,  # kPa
            "nu": 0.4,
        },
    ]

    # assign materials to elements (all elements use material_1 for now)
    element_materials = ["material_1"] * len(elements)

    # write to compute file
    write_compute_file(
        filename="INPUT.txt",
        solver_info=solver_info,
        stage_info=stage_info,
        assumption_info=assumption_info,
        materials=materials,
        coordinates=coordinates,
        node_numbers=node_numbers,
        elements=elements,
        element_materials=element_materials,
    )
