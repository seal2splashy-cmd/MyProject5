"""Generate a completely fictional, non-weapon, two-hand containment scanner GLB/OBJ.

Dependencies (only if regenerating): pip install numpy trimesh
The ZIP already contains the exported model, so ordinary Blender / Unreal users
need not run this script.

Local coordinates: X = left/right, -Y = forward, Z = up; metres.
Fit: designed for hands located near X=+/-0.125 m, Y~-0.92 m in AH operator v0.3.
"""
from pathlib import Path
import math
import numpy as np
import trimesh
from trimesh.visual.material import PBRMaterial

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / 'Assets' / 'Scanner'
OUT.mkdir(parents=True, exist_ok=True)

materials = {
    'graphite': PBRMaterial(name='M_AH_Scanner_Graphite', baseColorFactor=[42,49,54,255], metallicFactor=0.12, roughnessFactor=0.64),
    'rubber': PBRMaterial(name='M_AH_Scanner_Grip', baseColorFactor=[16,20,24,255], metallicFactor=0.02, roughnessFactor=0.86),
    'steel': PBRMaterial(name='M_AH_Scanner_Fastener', baseColorFactor=[103,111,118,255], metallicFactor=0.72, roughnessFactor=0.35),
    'screen': PBRMaterial(name='M_AH_Scanner_Screen', baseColorFactor=[8,44,55,255], metallicFactor=0, roughnessFactor=0.19, emissiveFactor=[0.015,0.10,0.15]),
    'warning': PBRMaterial(name='M_AH_Scanner_AmberIndicator', baseColorFactor=[240,151,45,255], metallicFactor=0, roughnessFactor=0.33, emissiveFactor=[0.8,0.28,0.01]),
    'signal': PBRMaterial(name='M_AH_Scanner_SeaGlassIndicator', baseColorFactor=[63,230,206,255], metallicFactor=0, roughnessFactor=0.3, emissiveFactor=[0.02,0.6,0.43]),
    'bezel': PBRMaterial(name='M_AH_Scanner_Bezel', baseColorFactor=[9,13,16,255], metallicFactor=0.2, roughnessFactor=0.5),
}
scene = trimesh.Scene()

def add(mesh, name, mat):
    mesh.visual = trimesh.visual.TextureVisuals(material=materials[mat])
    scene.add_geometry(mesh, geom_name=name, node_name=name)

def block(name, x, y, z, width, length, height, mat):
    m = trimesh.creation.box(extents=[width,length,height]); m.apply_translation([x,y,z]); add(m,name,mat)

def rounded_slab(name, center, width, length, depth, rad, mat, arc_points=5):
    x0,y0,z0=center
    # Clockwise rounded-square perimeter, triangulated lids and sealed sides.
    ring=[]
    for cx,cy,a0 in [ (width/2-rad,length/2-rad,0), (-width/2+rad,length/2-rad,90),(-width/2+rad,-length/2+rad,180),(width/2-rad,-length/2+rad,270) ]:
        for a in np.linspace(a0,a0+90,arc_points):
            t=math.radians(a)
            ring.append((cx+rad*math.cos(t),cy+rad*math.sin(t)))
    # Rotate perimeter if needed not important, fix normals at end.
    n=len(ring)
    vertices = [[x0+px,y0+py,z0-depth/2] for px,py in ring] + [[x0+px,y0+py,z0+depth/2] for px,py in ring]
    vertices.extend([[x0,y0,z0-depth/2],[x0,y0,z0+depth/2]])
    faces=[]
    for i in range(n):
        j=(i+1)%n
        faces.extend([[i,j,n+j],[i,n+j,n+i]])
        faces.extend([[2*n,j,i],[2*n+1,n+i,n+j]])
    mesh=trimesh.Trimesh(vertices=np.asarray(vertices),faces=faces,process=True)
    mesh.fix_normals(); add(mesh,name,mat)

def cylinder_z(name,x,y,z,radius,depth,mat,segments=20):
    m=trimesh.creation.cylinder(radius=radius,height=depth,sections=segments)
    m.apply_translation([x,y,z]); add(m,name,mat)

# Rugged twin-grip field instrument. Purely sensing/diagnostic equipment.
rounded_slab('Chassis_Main',(0,0,0),.208,.228,.053,.022,'graphite')
rounded_slab('Chassis_RearGasket',(0,.002,-.033),.188,.209,.008,.020,'rubber')
rounded_slab('Face_Bezel',(0,-.012,.031),.17,.155,.008,.018,'bezel')
rounded_slab('Display_Glass',(0,-.029,.037),.146,.112,.003,.011,'screen')
# Protective corners and twin cylindrical grip forms designed for gloved hands.
for side,tag in ((-1,'L'),(1,'R')):
    x=side*.135
    rounded_slab(f'Handle_{tag}',(x,-.024,-.004),.068,.179,.046,.021,'rubber')
    block(f'HandleRail_{tag}',side*.099,-.017,0,.028,.148,.024,'steel')
    for i in range(8):
        y=-.099+i*.019
        block(f'HandleRib_{tag}_{i:02}',x,y,.022,.064,.004,.003,'graphite')
    for iy in (-.098,.096):
        cylinder_z(f'Fastener_{tag}_{iy:+.2f}',side*.090,iy,.033,.0055,.004,'steel',12)
    for iy in (-.098,.096):
        rounded_slab(f'Bumper_{tag}_{iy:+.2f}',(x,iy,-.006),.065,.037,.055,.012,'rubber')
# Control strip between the hands, with diagnostic lamps.
rounded_slab('Readout_Trim',(0,.083,.039),.118,.034,.007,.006,'bezel')
for i in range(5):
    x=(i-2)*.021
    block(f'Diagnostic_Pip_{i}',x,.087,.044,.011,.013,.003,'signal' if i<3 else 'warning')
# Bolts on side frame, grilles, filter vent.
for i in range(6):
    x=(i-2.5)*.014
    block(f'Filter_Louver_{i}',x,-.103,.034,.007,.015,.004,'bezel')
for i in range(5):
    block(f'Screen_Grid_{i}',-.055+i*.025,-.025,.039,.001,.09,.001,'signal')
for i in range(4):
    block(f'Screen_Trace_{i}',0,-.077+i*.021,.039,.126,.001,.001,'signal')
# Housing seam and lower shock-absorbing pads.
rounded_slab('Base_ShockSeal',(0,0,-.038),.195,.213,.006,.020,'rubber')
for sy in (-1,1):
  for sx in (-1,1):
    block(f'CornerFoot_{sx}_{sy}',sx*.068,sy*.084,-.045,.034,.024,.009,'rubber')

out_glb=OUT/'SM_AH_ContainmentScanner.glb'
out_obj=OUT/'SM_AH_ContainmentScanner.obj'
scene.export(out_glb)
scene.export(out_obj)
print('scanner components',len(scene.geometry),'GLB bytes',out_glb.stat().st_size)
print(out_glb)
