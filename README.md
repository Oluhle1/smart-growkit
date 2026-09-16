# Smart GrowKit — Zovuyo Innovations

Shared workspace for developing the Smart GrowKit.

## Project structure

- firmware/: Device code and firmware setup — Oluhle.
- hardware/: Component list (BOM), photos, and pin connections — hardware partner.
- docs/: Requirements, decisions, setup guides, and testing notes.

## Working together

Keep files in their relevant folders.
Document wiring changes before updating the firmware.
Record important project decisions in docs/.

## Engineering drawings - Revision A

The CAD package is in [hardware/engineering](hardware/engineering/).

- [Dimensioned engineering drawings (PDF)](hardware/engineering/ZI_SGK_Engineering_Drawings_RevA.pdf)
- [3D assembly (STEP)](hardware/engineering/cad/ZI_SGK_Assembly_RevA.step)
- [Frame members (STEP)](hardware/engineering/cad/ZI_SGK_Frame_RevA.step)
- [2D DXF drawings and flat profiles](hardware/engineering/cad/)
- [Design basis and engineering review holds](hardware/engineering/README.md)

**Status: engineering review, not released for fabrication.**
The proposed structure is 600 W x 450 D x 2000 H mm, with three tiers and
12 plant positions. Confirm purchased-component dimensions, connections,
structural capacity, stability and hydraulic performance before fabrication.

The package guide was supplied with an original ZIP containing `CAD/` and
`Source/` folders. In this repository, the uploaded models and drawings are
under lowercase `cad/`; regeneration scripts have not yet been added here.
Use the links above to open the files. STEP and DXF files can be viewed without
running Python. For a standalone copy of a file, open it on GitHub and use its
download button.
