# File to write the material properties, mesh, etc to the compute file

import os


def write_compute_file(
    filename, domain_info, material, coordinates, node_numbers, elements
):

    base_dir = os.path.dirname(__file__)
    target_dir = os.path.join(base_dir, "../Engine/Input")
    os.makedirs(target_dir, exist_ok=True)
    full_path = os.path.join(target_dir, filename)
    f = open(full_path, "w")

    # write domain information
    f.write("domain:" + "\n")
    f.write(" ")
    f.write(
        "width: "
        + str(domain_info[0])
        + " height: "
        + str(domain_info[1])
        + " el_size: "
        + str(domain_info[2])
        + "\n"
    )
    f.write("\n")

    # write materials
    f.write("materials:" + "\n")
    f.write(" ")
    f.write("E: " + str(material[0]) + " nu: " + str(material[1]) + "\n")
    f.write("\n")

    # write node numbers and corresponding coordinates
    f.write("nodes:" + "\n")
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
    for i, element in enumerate(elements):
        f.write(" ")
        f.write("element: " + str(i + 1) + " nodes: " + str(element))
        f.write("\n")
    f.write("\n")

    f.close()
