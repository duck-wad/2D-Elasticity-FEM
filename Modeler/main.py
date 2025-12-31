# File to run model setup, mesh generation, compute file writing

from mesh import *
from write_compute_file import *

if __name__ == "__main__":

    # define general information
    solver = "GAUSSIAN_ELIMINATION"
    tolerance = 0.001
    maxiter = 500
    stages = 1
    solver_info = [solver, tolerance, maxiter]
    stage_info = [stages]

    # define domain size and element size
    domain_width = 10
    domain_height = 10
    element_size = 3

    # generate the mesh
    coordinates, node_numbers, elements = generate_quad_mesh(
        domain_width, domain_height, element_size
    )

    # define material properties
    E = 20000  # kPa
    nu = 0.2
    material = [E, nu]

    # write to compute file
    write_compute_file(
        filename="INPUT.txt",
        solver_info=solver_info,
        stage_info=stage_info,
        material=material,
        coordinates=coordinates,
        node_numbers=node_numbers,
        elements=elements,
    )
