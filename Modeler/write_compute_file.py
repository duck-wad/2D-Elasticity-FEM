# File to write the material properties, mesh, etc to the compute file

import os


def write_compute_file(
    filename, solver_info, stage_info, material, coordinates, node_numbers, elements
):

    base_dir = os.path.dirname(__file__)
    target_dir = os.path.join(base_dir, "../Engine/Input")
    os.makedirs(target_dir, exist_ok=True)
    full_path = os.path.join(target_dir, filename)
    f = open(full_path, "w")

    # write general information
    f.write("general:" + "\n")
    f.write(
        " solver: "
        + solver_info[0]
        + " tolerance: "
        + str(solver_info[1])
        + " maxiter: "
        + str(solver_info[2])
        + "\n"
    )
    f.write(" stages: " + str(stage_info[0]) + "\n")
    f.write("\n")

    # write materials
    f.write("materials:" + "\n")
    f.write(" ")
    f.write("E: " + str(material[0]) + " nu: " + str(material[1]) + "\n")
    f.write("\n")

    # write node numbers and corresponding coordinates
    f.write("nodes:" + "\n")
    f.write(" numnodes: " + str(len(node_numbers)) + "\n")
    f.write("\n")
    for i, node in enumerate(node_numbers):
        f.write(" ")
        f.write(
            "node: "
            + str(node)
            + " x: "
            + str(coordinates[i][0])
            + " y: "
            + str(coordinates[i][1])
        )
        f.write("\n")
    f.write("\n")

    # write elements and their corresponding nodes
    f.write("elements:" + "\n")
    f.write(" numelem: " + str(len(elements)) + " type: Q4" + "\n")
    f.write("\n")
    for i, element in enumerate(elements):
        f.write(" ")
        f.write("element: " + str(i + 1) + " nodes: " + str(element))
        f.write("\n")
    f.write("\n")

    f.close()
