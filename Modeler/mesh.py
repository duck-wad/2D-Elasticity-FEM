# File to generate a mesh of elements given a rectangular domain

import numpy as np


# Given a rectangular 2D domain defined by a height and width as well as a desired element size,
# generate a structured grid of quadrilateral elements
# width is x dimension height is y dimension
def generate_quad_mesh(height: float, width: float, elsize: float):
    if elsize > 0.5 * height or elsize > 0.5 * width:
        raise ValueError(
            "Element size must be less than half the size of the domain edge"
        )

    """ GENERATE MESH OF NODES """

    width_div = int(width / elsize)
    height_div = int(height / elsize)

    width_arr = np.linspace(0, width, width_div + 1)
    height_arr = np.linspace(0, height, height_div + 1)

    # create every possible combination from the width and height point arrays
    mesh_x, mesh_y = np.meshgrid(width_arr, height_arr)
    coordinates = np.stack([mesh_x.ravel(), mesh_y.ravel()], axis=-1)
    coordinates = [tuple(row) for row in coordinates]
    # corresponding list of node numbers
    node_numbers = np.linspace(1, len(coordinates), len(coordinates))
    node_numbers = node_numbers.astype(int)
    node_numbers = node_numbers.tolist()

    """ GENERATE LIST OF ELEMENTS FROM NODES """

    # generate list of element points starting from bottom left corner of domain, and moving right and upwards
    # node numbering starts in bottom left corner of element and counter clockwise
    elements = []
    for row in range(height_div):
        for col in range(width_div):
            elements.append(
                (
                    node_numbers[(row * (width_div + 1)) + col],
                    node_numbers[(row * (width_div + 1)) + col + 1],
                    node_numbers[((row + 1) * (width_div + 1)) + col + 1],
                    node_numbers[((row + 1) * (width_div + 1)) + col],
                )
            )

    return coordinates, node_numbers, elements


# Function to visualize the created mesh
def visualize_mesh(elements):
    pass
