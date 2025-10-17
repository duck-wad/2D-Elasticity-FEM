# File to run model setup, mesh generation, compute file writing

from mesh import *
from write_compute_file import *

if __name__ == "__main__":

    # define domain size and element size
    domain_width = 10
    domain_height = 10
    element_size = 3
    domain_info = [domain_width, domain_height, element_size]

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
        domain_info=domain_info,
        material=material,
        coordinates=coordinates,
        node_numbers=node_numbers,
        elements=elements,
    )
