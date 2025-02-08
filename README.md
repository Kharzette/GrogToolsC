# GrogToolsC
Tools for making game content

My existing dotnet tools are a pain to get going on linux because of the winforms dependencies.  This is an effort to split out the gui bits into something cross platform.

I'll start with ColladaConvert.

# ColladaConvertC
This is working fairly well now for characters, but needs a lot of work for statics.

For now I've decided to leave character bone space in right handed with root nodes converting to left.  This is working so far but the bone collision shapes need some serious testing to make sure they are where they are drawing.

# Character Process
To get a character ready to load by GrogLibs, it is best to use a Blender no newer than 3.6 to ensure that it works with [GameRig](https://github.com/SAM-tak/BlenderGameRig)

Build your mesh in ordinary human sizes.  I like 1.7ish meters or 94 valve units.

# Materials
I am using 3DCoat 3 for texture painting.  I'll try to get around to doing instructions for keeping it in blender in the future.

3DCoat->Import object for pixel painting:  Your base mesh DAE
	UV auto mapping
	swap Y and Z

Go to the UV tab and adjust the islands the way you want.  Then go to paint and start painting away.  Use layers!

When finished do File->Export model:  (this will have the new UVs)
Export low-poly mesh
Swap Y and Z
Export Color

Save as FBX.  This will have to go back into blender as 3DCoat's collada is a bit mangled.  When importing back into blender the scale is super small.  Fix it in the little transform tab.

Rotate Z 90 degrees positive to align it the usual way if needed, then Object->Apply->Rotation.

Note that as of this writing, there is a bug in Blender that starts the "set" of UV coords at 2 instead of 0.  I have a hack in place but only 1 coordinate set works right now.

# Rigging
In object mode do:  add->armature->gamerig->unity mechanim->human meta rig.  Zero out the transform in the transform panel.

Delete extra finger bones, but don't delete the heel.  Any fingers you keep must have at least 2 bones!

Adjust the sizes of the bones to fit your mesh in edit mode.  This is best done by using the move tool with the spherical joints.  Pulling those adjusts the overall bone.  Do one side, then select the adjusted bones and do Armature->Symmetrize to copy to the other side.

Knees must be slightly bent!

For extra bones, Add->single bone, then move where you want it.  Select the new bone, then the bone you want to be its parent, then:  Armature->Parent->Make->Keep Offset

You can Armature->Symmetrize with the new bone selected if you need another on the other side.

When it looks about right, go back to object mode:

Select the mesh, then the metarig.  Do this with clicks in the viewport as the scene collection causes it to bug out.  Then Object->Parent->Armature Deform->With Auto Weights.  Auto weights work pretty well with 3.6, but it is good to check influences:

To do this go into edit mode on the mesh, and go to data (the green triangle).  This will list vertex groups.  You can click one and hit select below and see which bones are tied to which verts.  This is handy for removing like left thigh bone influencing the right knee etc...

When the weights look right do: Object Data Properties panel->GameRig->Generate New Rig

There used to be a problem with generated rigs having a 90 degree rotation in them, but I think I've gotten rid of that now.

Select the generated rig and do Object->Apply->Rotation just in case.

Unparent the mesh from the metarig and parent it to the generated rig.  This time, select the mesh first, then the generated rig.  Why?  Who knows!?

Make sure a material is assigned, even if you don't use it.

Export the base DAE.  Select just the mesh, then in export:

Main tab:
	Selection only
	Include Armatures
	Global Orientation
		Leave Default:
			Forward	Y	Up	Z
		Check Apply
Geom tab:
	No triangulate
	Transform: Matrix
Arm tab:
	Deform bones only
Anim Tab:
	Include Animations
	Samples (would prefer curves but they just give you blank anims)
	Include all Actions
	Transform Matrix

# Animating
So you HAVE to use the hotkey to generate keys.  Everything you see on the web will say I, but in industry standard mode this is S.  Remember this and save yourself hours of fruitless googling.  The various key related menus do NOTHING.

Watch for keys not exporting.  Sometimes there will be a problem bone (usually hips) that won't export so if you are leaning into a run it looks like your character is floating backward when exported.  There seems no way to fix this other than trash the entire anim and start over.  Hey, it's free!
