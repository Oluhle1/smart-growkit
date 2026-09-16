# Zovuyo Smart GrowKit engineering drawing package

Revision A | 15 September 2026 | Document family ZI-SGK-M001 to M010

**Issue status: engineering review. Not released for fabrication, CNC cutting or construction.**

This is a new dimensioned mechanical proposal derived from the supplied references. It is not a measurement of an existing unit. The source-of-truth document explicitly leaves the final envelope, reservoir capacity, bought-out components and mechanical validation open. The engineering team should close the named release holds and issue a checked revision before fabrication.

## Open these first

- `ZI_SGK_Engineering_Drawings_RevA.pdf`: ten A3 landscape sheets with dimensions, cut schedule, mounting reserves, hydraulic layout and review holds. Print at A3, 100%; do not measure a scaled print to obtain a missing dimension.
- `CAD/ZI_SGK_Assembly_RevA.step`: named assembly solids in millimetres for importing into a STEP-capable CAD system. Supplier extrusion slots, fasteners and final plumbing fittings are not detailed. Tank, electronics, fans, sensor pods and lights are reserved envelopes.
- `CAD/ZI_SGK_Frame_RevA.step`: frame members alone; excludes braces, feet, connection hardware and other equipment. It is a member-layout model, not a structural analysis model.
- `CAD/ZI_SGK_M001_RevA.dxf` through `M010`: editable drawing sheets. Unbroken dimensioned views are full-size in modelspace. Sheet borders and annotation are multiplied by the stated plot-scale denominator. For example, a 600 mm width on an M002 view measures 600 drawing units; its A3 sheet border spans 4200 x 2970 units for plotting at 1:10. Units are mm. Schematics, the broken brace detail and the M001 overview are exceptions: use their labels. The M001 overview image appears in the PDF only.
- `CAD/ZI_SGK_Flat_Profiles_Review_RevA.dxf`: full-length nominal flat-part profiles in mm, including exact CAD arcs for slots and rounded brace ends. These are proposals, not approved toolpaths.

DXF files are interchange drawings, not a proprietary DWG. STEP files contain solids, not a vendor-specific editable feature history. The Python source provides regeneration and controlled design editing.

## Sheet register

| Sheet | Content | Plotted scale |
|---|---|---|
| M001 | Basis, illustrated assembly and source conflicts | NTS |
| M002 | General arrangement, dimensions and service clearances | 1:10 |
| M003 | Frame members, bracing locations and cut schedule | 1:10 |
| M004 | Tier arrangement, plant centres, supports and lighting | 1:3 |
| M005 | Channel, lid and hydroponic interfaces | 1:2 |
| M006 | Tank bay, spill tray, electronics barrier and access | 1:3 |
| M007 | IoT reserves, sensing locations and harness separation | 1:2, internal zones schematic |
| M008 | Parallel feed and gravity-return hydraulic scheme | NTS |
| M009 | Saddle plate, broken brace detail and structural criteria | 1:1, brace length broken |
| M010 | Assembly sequence, procurement schedule and release holds | NTS |

## Design basis and changes

Equipment envelope: 600 W x 450 D x 2000 H mm at nominal foot height. This excludes plant canopy, external mains adapter, wall restraints and service access. Two 400 x 100 channels per tier with two plants in each: 12 plant sites. Front channel underside heights are 550, 1050 and 1550 mm. Each channel rises 8 mm towards its rear. Nominal tank capacity remains 20 L; the tank box in STEP is an allowed external envelope, not a capacity model.

All new mechanical dimensions are proposals. `[S]` denotes a supplied requirement, which may itself be provisional in the source. `[P]` denotes a proposal. `HOLD` means the engineer must resolve an interface or validate evidence before cutting or approving that feature.

The current request asks for CO2 monitoring. This package therefore reserves it, although SSOT v1.0 deferred it. It does not include CO2 injection. It preserves manual pH/nutrient correction. Parallel feed is proposed in place of the older concept's cascade, and all three tiers use NFT in line with the controlling document. The older 10 A fuse and open enclosure ventilation suggestions are not adopted as approved specifications.

This is an open-frame garden. Fans can circulate room air; the unit cannot hold independent chamber temperature, humidity or CO2 concentration without a separate enclosure and climate-control design. General air quality, VOCs, particulates and air velocity are not implied by temperature/RH and CO2 sensing.

## Engineering holds

1. Confirm the selected extrusion section, connections, bracing, foot adaptors and restraint loads. Check full/empty-tank stability and connection slip. Proposed loads and deflection limits are design inputs, not proven load ratings.
2. Measure the tank, enclosure, pots, probes, LEDs, fans and all plumbing fittings. Finalise fixing holes, thread engagement, gland bores and physical access.
3. Prove channel wetting, pot/root clearance, pumping at delivery head, return capacity and obstruction response. Measure drain-back and calibrate the maximum fill mark; 14 L starting fill and 16 L maximum operating target remain commissioning proposals.
4. Reconcile exact sensor parts, added CO2 monitoring, power budget, branch protection and thermal performance with SSOT section 11. Electrical schematics and firmware are separate deliverables, not inferred from this mechanical package.
5. Record product-owner acceptance and update the source-of-truth decision log. Run integrated leak/flow/stability checks, offline/fault tests and seven-day soak before unattended crop trials.

The spill tray is for limited spills/leak detection and does not contain the whole reservoir. External certified mains-to-12 V conversion stays outside the frame. No enclosed wet-zone mains wiring is designed here.

## Verification performed

- Generated CAD assembly has 101 valid solids and bounds X=0-600, Y=0-450, Z=0-2000 mm.
- Frame member schedule: four 1950 mm posts, fifteen 540 mm members and ten 390 mm members, 29 pieces / 19.8 m net. Stock cutting must allow for saw kerf and end trimming.
- Checked six channel bodies, 12 plant sites, 500 mm tier spacing and 8/400 = 2% channel fall.
- Targeted geometric interference check found no positive-volume overlaps between the main feed/return reserves, tank and enclosure and the tested posts, rails, braces, deck and splash barrier. This is not an exhaustive collision review. Flexible connections and unselected fixing hardware are not represented; saddles and components use space reserves.
- Reopened the STEP export and audited DXF files for interchange integrity. Visually reviewed the PDF sheets.
- No physical load, stability, leak, flow, electrical, thermal or crop trial has been performed by this work.

## Sources and precedence

1. `Zovuyo_Smart_GrowKit_Source_of_Truth_v1.0.docx`, controlling working baseline dated 30 August 2026; especially sections 3, 11 and 14.
2. `Design Summary.pdf`, 5 February 2026; pages 6-7 establish the earlier three-tier visual concept. Earlier electrical suggestions are subordinate to the source of truth.
3. `Screen Recording 2026-02-05 041500.mp4`, 31.53 seconds; sampled for visual form and arrangement. It supplies no verified manufacturing dimensions.
4. Oregon State University Extension, [Hydro hints: Nutrient film technique, EM 9457](https://extension.oregonstate.edu/catalog/pub/em-9457-hydro-hints-nutrient-film-technique). Used for the 2% slope starting point and the importance of sustained flow.
5. Virginia Cooperative Extension, [Hydroponic Production of Edible Crops: Nutrient Film Technique Systems, SPES-463NP](https://www.pubs.ext.vt.edu/SPES/spes-463/spes-463.html). Used for continuous-duty pump selection at delivery head and initial channel-flow commissioning range, converted from 3-5 US gal/h to approximately 0.19-0.32 L/min. Final operation requires crop and hydraulic tests.

## Regeneration

The delivered sources were run with Python 3.12, CadQuery 2.7.0 and ezdxf 1.4.4, with ReportLab and Matplotlib. From any working directory:

```bash
python Source/render_assembly.py
python Source/build_flat_profiles.py
python Source/build_drawings.py
```

`render_assembly.py` calls `build_model.py`, which recreates both STEP files, the component register, geometry checks and a hidden-line SVG, then creates the shaded PNG used in the PDF. `build_drawings.py` recreates the ten sheet DXFs and PDF. Geometry changes require coordinated updates to model, drawing dimensions and design notes; the scripts are not a complete associative CAD constraint system.
