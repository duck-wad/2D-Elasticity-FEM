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
    thickness = 0.069444
    assumption_info = [assumption, thickness]

    # define domain size and element size
    domain_width = 0.12
    domain_height = 1
    element_size = 0.02

    # generate the mesh
    coordinates, node_numbers, elements = generate_quad_mesh(
        domain_width, domain_height, element_size
    )

    # define material properties
    materials = [
        {
            "name": "material_1",
            "formulation": "LinearElastic",
            "E": 200000000,  # Pa
            "nu": 0.29,
        },
    ]

    # assign materials to elements (all elements use material_1 for now)
    element_materials = ["material_1"] * len(elements)

    # write to compute file
    write_compute_file(
        filename="INPUTTT.txt",
        solver_info=solver_info,
        stage_info=stage_info,
        assumption_info=assumption_info,
        materials=materials,
        coordinates=coordinates,
        node_numbers=node_numbers,
        elements=elements,
        element_materials=element_materials,
    )
