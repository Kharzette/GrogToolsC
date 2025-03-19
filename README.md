# GrogToolsC
Tools for making game content

My existing dotnet tools are a pain to get going on linux because of the winforms dependencies.  This is an effort to split out the gui bits into something cross platform.

I'll start with ColladaConvert.

# ColladaConvertC
I've now ditched Collada in favour of the gltf format.  Surprise!

For now I've decided to leave character bone space in right handed with the root node converting to left.  This is working so far but the bone collision shapes need some serious testing to make sure they are where they are drawing.

# Character Process
To get a character ready to load by GrogLibs, use a Blender that works with [GameRig](https://github.com/SAM-tak/BlenderGameRig).  I am currently using 4.3.2.

Build your mesh in ordinary human sizes.  I like 1.7ish meters or 94 valve units.

# Materials
I am using 3DCoat 3 for texture painting.  I'll try to get around to doing instructions for keeping it in blender in the future.

Export the character mesh as FBX.  Default settings seem fine.

3DCoat->Import object for pixel painting:  Your base mesh fbx
	UV auto mapping

Go to the UV tab and adjust the islands the way you want.  Then go to paint and start painting away.  Use layers!

When finished do File->Export model:  (this will have the new UVs)
Export low-poly mesh
Export Color

Save as FBX.  After importing do Object->Apply->All Transforms.

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

Export the base mesh.  Hide any extra stuff in the scene like cameras and lights.  Export->glTF2

Format->glTF Separate
Check "Remember Export Settings"
Include->Limit to->Visible Objects
Materials
	No Export (dropdown)
Shape Keys->Uncheck (don't support those yet)
Animation->Uncheck (this is just the character so far)

# Animating
In the past I had trouble with keys not exporting, but I now think that was down to the Collada exporter.  Everything so far in gltf just works.

For exporting, use Export->glTF2 as above with the base mesh with:
Data->Armature->Export Deformation Bones Only->check
Animation->check
Sampling Animations->check (the other stuff doesn't seem to work yet)
Optimize
	Optimize Size->check (this is almost as good as pure key export)
	Force keeping channels for bones->check

The deformation only check above prevents export of all the extra IK and junk from gamerig.  It might cause trouble later on if a character has attachment points, like for weapons or bags or whatever.  In that case, the extra stuff can be deleted from the scene, export, undo delete stuff.

# Importing
Load gltf Char will load the base mesh.  Load gltf Anim loads anims.  There may be trouble in the future with bones having different index values between files, but I haven't seen it yet.  It seems like a thing that could happen just from looking at the file format.  I had code to deal with it but no way to test it so I got rid of it.

# Bone Collision
The bone editing stuff is using in-render-window gui and slides out with the F2 key.  After that most keys will be showin in the info panel at the bottom (and will likely change as they are goofy right now).