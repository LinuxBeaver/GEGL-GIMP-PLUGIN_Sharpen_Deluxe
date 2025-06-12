/* This file is an image processing operation for GEGL
 *
 * GEGL is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * GEGL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GEGL; if not, see <https://www.gnu.org/licenses/>.
 *
 * Credit to Øvind Kolas (pippin) for major GEGL contributions
 * 2023 Beaver, Deluxe Sharpen 

This is a plugin for Gimp that sharpens images in various ways.
 
Pasting this syntax in Gimp's GEGL Graph filter will generate a static preview of this plugin. So users can test without installing.

id=1 
gimp:layer-mode layer-mode=grain-extract blend-space=rgb-perceptual aux=[ ref=1 median-blur radius=10]
gimp:layer-mode layer-mode=grain-merge blend-space=rgb-perceptual aux=[  ref=1  ]
end of syntax 

Replace Median Blur with other filters like `denoise-dct sigma=30` `mean-curvature-blur iterations=20` `box radius=10`
to see different types of high pass sharpens that this filter offers.


all sharpens are made by grain extracting blurs and anisotropic filters

fun fact, this plugin was inspired by the first GEGL plugin I ever made bace in May 2022, when I swapped high pass's "gaussian-blur" with "mean-curvature-blur' 
this was during my early days of studying GEGL's code. Said original plugin is removed for being too simple. But it got a lot of attention on Gimp Chat
as the first third party GEGL plugin ever. If you want to test what my first GEGL Plugin ever made was like set the sharpen to "Mean Curvature.
 */


#include "config.h"
#include <glib/gi18n-lib.h>

#ifdef GEGL_PROPERTIES

property_enum (type, _("Type of Sharpen"),
    sharpenmeme, sharpen_meme,
    median)
    description (_("Choose a sharpen mask. The filter selected is 'grain extracted' to make it a high pass sharpen mask."))


enum_start (sharpen_meme)
  enum_value (median,      "median",
              N_("Median"))
  enum_value (denoise,      "denoise",
              N_("Denoise"))
  enum_value (box,      "box",
              N_("Box"))
  enum_value (mcb,      "mcb",
              N_("Mean Curvature"))
  enum_value (domainsmooth,      "ds",
              N_("Domain Smooth"))
  enum_value (noisereduction,      "nr",
              N_("Noise Reduction"))
  enum_value (lens,      "lens",
              N_("Lens"))
  enum_value (gaussian,      "gaussian",
              N_("Gaussian"))
enum_end (sharpenmeme)

property_int  (median, _("Median Sharpen"), 11)
  value_range (0, 35)
  ui_range    (0, 35)
  ui_meta     ("unit", "pixel-distance")
  description (_("Median High Pass"))
ui_meta ("visible", "type {median}" )

/*ui_meta visible is tagged on many GUI options so the
sliders only display when their corresponding filter is 
selected from the list. In this case, this slider will
only appear when Median Blur is selected.*/

enum_start (median_blur_shape_sharp)
  enum_value (GEGL_MEDIAN_BLUR_NEIGHBORHOOD_SQUARE,  "square",  N_("Square"))
  enum_value (GEGL_MEDIAN_BLUR_NEIGHBORHOOD_CIRCLE,  "circle",  N_("Circle"))
  enum_value (GEGL_MEDIAN_BLUR_NEIGHBORHOOD_DIAMOND, "diamond", N_("Diamond"))
enum_end (medianblurshapesharp)


property_enum (medianshape, _("Median Shape"),
               medianblurshapesharp, median_blur_shape_sharp,
               GEGL_MEDIAN_BLUR_NEIGHBORHOOD_CIRCLE)
  description (_("Base shape of Median"))
ui_meta ("visible", "type {median}" )


property_int (box, _("Box Sharpen"), 3)
   description(_("Box Blur High Pass"))
   value_range (0, 11)
ui_meta ("visible", "type {box}" )

property_double (denoise, _("Denoise Sharpen"), 40.0)
    description (_("Denoise High Pass"))
    value_range (20.0, 60.0)
ui_meta ("visible", "type {denoise}" )

property_double (mcb, _("Mean Curvature Sharpen"), 30.0)
    description (_("Mean Curvature High Pass"))
    value_range (5.0, 80.0)
ui_meta ("visible", "type {mcb}" )


property_double (ds, _("Domain Smooth Sharpen (reverse direction)"),  0.8)
    description (_("Domain Smooth High Pass"))
  value_range (0.5, 0.9)
ui_meta ("visible", "type {ds}" )


property_double (nr, _("Noise Reduction Sharpen"), 25.0)
    description (_("Noise Reduction High Pass. This mode barely does any noticable changes."))
    value_range (15.0, 30.0)
ui_meta ("visible", "type {nr}" )

property_double (lens, _("Lens Blur Sharpen"), 11.0)
  description (_("Lens Blur High Pass"))
  value_range (0.0, 11.0)
  ui_range    (0.0, 11.0)
  ui_gamma    (2.0)
ui_meta ("visible", "type {lens}" )

property_double (gaussian, _("Gaussian Blur Sharpen (default high pass)"), 5.0)
  description (_("Gaussian Blur High Pass. This is pretty much the same thing as Gimp's default high pass filter."))
  value_range (0.0, 10.0)
  ui_range    (0.0, 10.0)
  ui_gamma    (2.0)
ui_meta ("visible", "type {gaussian}" )

property_boolean (autograinmerge, _("Auto Grain Merge (sharpen instantly)"), FALSE)
  description    (_("Auto Switch to the Grain Merge blend mode. This will turn the high pass into a sharpen effect without needing to select blend modes. When using this make sure you have the default replace blend mode selected."))

#else

#define GEGL_OP_META
#define GEGL_OP_NAME     sharpendeluxe
#define GEGL_OP_C_SOURCE sharpendeluxe.c

#include "gegl-op.h"


typedef struct
{
  GeglNode *input;
  GeglNode *mcb;
  GeglNode *grainextract;
  GeglNode *box;
  GeglNode *nop1;
  GeglNode *nop2;
  GeglNode *grainmerge;
  GeglNode *median;
  GeglNode *denoise;
  GeglNode *ds;
  GeglNode *lens;
  GeglNode *gaussian;
  GeglNode *nr;
  GeglNode *output;
} State; 


static void attach (GeglOperation *operation)
{
  GeglNode *gegl = operation->node;
GeglProperties *o = GEGL_PROPERTIES (operation);
  GeglNode *input, *output, *grainextract, *box, *ds, *nr, *mcb, *gaussian, *lens, *denoise, *nop1, *nop2, *grainmerge, *median;


  input    = gegl_node_get_input_proxy (gegl, "input");
  output   = gegl_node_get_output_proxy (gegl, "output");

grainextract = gegl_node_new_child (gegl,
                              "operation", "gimp:layer-mode", "layer-mode", 46, "composite-mode", 0, "blend-space", 2, NULL);
/*This is code for Grain Extract at the RGB Peceptual blend space. (gimp:layer-mode layer-mode=grain-extract blend-space=rgb-perceptual ) if Gimp ever gets
new blend modes then the number will be off by one number (45 or 47) and someone will have to manually update it. 

I discovered that for some odd reason the grain extract did not work proper unless blend space was set to  for rgb-perceptual. rgb-linear had a different effect
then Gimp in default..

Also this is a good example of why Gimp only blend modes should be used as they save time.*/


 median   = gegl_node_new_child (gegl,
                                  "operation", "gegl:median-blur", "alpha-percentile", 100.0, "percentile", 50.0,
                                  NULL);

 denoise   = gegl_node_new_child (gegl,
                                  "operation", "gegl:denoise-dct",
                                  NULL);

 box   = gegl_node_new_child (gegl,
                                  "operation", "gegl:box-blur",
                                  NULL);

 nop1   = gegl_node_new_child (gegl,
                                  "operation", "gegl:nop",
                                  NULL);

 nop2   = gegl_node_new_child (gegl,
                                  "operation", "gegl:nop",
                                  NULL);

grainmerge = gegl_node_new_child (gegl,
                              "operation", "gimp:layer-mode", "layer-mode", 47, "composite-mode", 0, "blend-space", 2, NULL);



 mcb   = gegl_node_new_child (gegl,
                                  "operation", "gegl:mean-curvature-blur",
                                  NULL);

 ds   = gegl_node_new_child (gegl,
                                  "operation", "gegl:domain-transform", "spatial-factor", 100.0,
                                  NULL);

 nr   = gegl_node_new_child (gegl,
                                  "operation", "gegl:noise-reduction",
                                  NULL);

 lens   = gegl_node_new_child (gegl,
                                  "operation", "gegl:lens-blur",
                                  NULL);

 gaussian   = gegl_node_new_child (gegl,
                                  "operation", "gegl:gaussian-blur",
                                  NULL);

  gegl_operation_meta_redirect (operation, "box", box, "radius");
  gegl_operation_meta_redirect (operation, "median", median, "radius");
  gegl_operation_meta_redirect (operation, "medianshape", median, "neighborhood");
  gegl_operation_meta_redirect (operation, "denoise", denoise, "sigma");
  gegl_operation_meta_redirect (operation, "mcb", mcb, "iterations");
  gegl_operation_meta_redirect (operation, "ds", ds, "edge-preservation");
  gegl_operation_meta_redirect (operation, "nr", nr, "iterations");
  gegl_operation_meta_redirect (operation, "lens", lens, "radius");
  gegl_operation_meta_redirect (operation, "gaussian", gaussian, "std-dev-x");
  gegl_operation_meta_redirect (operation, "gaussian", gaussian, "std-dev-y");

  State *state = g_malloc0 (sizeof (State));
  o->user_data = state;
  state->input = input;
  state->grainextract = grainextract;
  state->box = box;
  state->mcb = mcb;
  state->median = median;
  state->denoise = denoise;
  state->ds = ds;
  state->nr = nr;
  state->lens = lens;
  state->gaussian = gaussian;
  state->nop1 = nop1;
  state->nop2 = nop2;
  state->grainmerge = grainmerge;
  state->output = output;

  o->user_data = state;
}

static void
update_graph (GeglOperation *operation)
{
  GeglProperties *o = GEGL_PROPERTIES (operation);
  State *state = o->user_data;
  if (!state) return;

if (o->autograinmerge) 


switch (o->type) {
        break;
    case median:
            gegl_node_link_many (state->input, state->grainextract, state->grainmerge, state->output, NULL);
            gegl_node_connect (state->grainextract, "aux", state->median, "output");
            gegl_node_link_many (state->input, state->median, NULL);
            gegl_node_connect (state->grainmerge, "aux", state->input, "output");
        break;
    case denoise:
            gegl_node_link_many (state->input, state->grainextract, state->grainmerge, state->output, NULL);
            gegl_node_connect (state->grainextract, "aux", state->denoise, "output");
            gegl_node_link_many (state->input, state->denoise, NULL);
            gegl_node_connect (state->grainmerge, "aux", state->input, "output");
        break;
    case box:
            gegl_node_link_many (state->input, state->grainextract, state->grainmerge, state->output, NULL);
            gegl_node_connect (state->grainextract, "aux", state->box, "output");
            gegl_node_link_many (state->input, state->box, NULL);
            gegl_node_connect (state->grainmerge, "aux", state->input, "output");
        break;
    case mcb:
            gegl_node_link_many (state->input, state->grainextract, state->grainmerge, state->output, NULL);
            gegl_node_connect (state->grainextract, "aux", state->mcb, "output");
            gegl_node_link_many (state->input, state->mcb, NULL);
            gegl_node_connect (state->grainmerge, "aux", state->input, "output");
        break;
    case domainsmooth:
            gegl_node_link_many (state->input, state->grainextract, state->grainmerge, state->output, NULL);
            gegl_node_connect (state->grainextract, "aux", state->ds, "output");
            gegl_node_link_many (state->input, state->ds, NULL);
            gegl_node_connect (state->grainmerge, "aux", state->input, "output");
        break;
    case noisereduction:
            gegl_node_link_many (state->input, state->grainextract, state->grainmerge, state->output, NULL);
            gegl_node_connect (state->grainextract, "aux", state->nr, "output");
            gegl_node_link_many (state->input, state->nr, NULL);
            gegl_node_connect (state->grainmerge, "aux", state->input, "output");
        break;
    case lens:
            gegl_node_link_many (state->input, state->grainextract, state->grainmerge, state->output, NULL);
            gegl_node_connect (state->grainextract, "aux", state->lens, "output");
            gegl_node_link_many (state->input, state->lens, NULL);
            gegl_node_connect (state->grainmerge, "aux", state->input, "output");
        break;
    case gaussian:
            gegl_node_link_many (state->input, state->grainextract, state->grainmerge, state->output, NULL);
            gegl_node_connect (state->grainextract, "aux", state->gaussian, "output");
            gegl_node_link_many (state->input, state->gaussian, NULL);
            gegl_node_connect (state->grainmerge, "aux", state->input, "output");
    }

else 

switch (o->type) {
        break;
    case median:
            gegl_node_link_many (state->input, state->grainextract, state->output, NULL);
            gegl_node_connect (state->grainextract, "aux", state->median, "output");
            gegl_node_link_many (state->input, state->median, NULL);
        break;
    case denoise:
            gegl_node_link_many (state->input, state->grainextract, state->output, NULL);
            gegl_node_connect (state->grainextract, "aux", state->denoise, "output");
            gegl_node_link_many (state->input, state->denoise, NULL);
        break;
    case box:
            gegl_node_link_many (state->input, state->grainextract, state->output, NULL);
            gegl_node_connect (state->grainextract, "aux", state->box, "output");
            gegl_node_link_many (state->input, state->box, NULL);
        break;
    case mcb:
            gegl_node_link_many (state->input, state->grainextract, state->output, NULL);
            gegl_node_connect (state->grainextract, "aux", state->mcb, "output");
            gegl_node_link_many (state->input, state->mcb, NULL);
        break;
    case domainsmooth:
            gegl_node_link_many (state->input, state->grainextract, state->output, NULL);
            gegl_node_connect (state->grainextract, "aux", state->ds, "output");
            gegl_node_link_many (state->input, state->ds, NULL);
        break;
    case noisereduction:
            gegl_node_link_many (state->input, state->grainextract, state->output, NULL);
            gegl_node_connect (state->grainextract, "aux", state->nr, "output");
            gegl_node_link_many (state->input, state->nr, NULL);
        break;
    case lens:
            gegl_node_link_many (state->input, state->grainextract, state->output, NULL);
            gegl_node_connect (state->grainextract, "aux", state->lens, "output");
            gegl_node_link_many (state->input, state->lens, NULL);
        break;
    case gaussian:
            gegl_node_link_many (state->input, state->grainextract, state->output, NULL);
            gegl_node_connect (state->grainextract, "aux", state->gaussian, "output");
            gegl_node_link_many (state->input, state->gaussian, NULL);
}
    }
      


  

static void
gegl_op_class_init (GeglOpClass *klass)
{
  GeglOperationClass *operation_class;

  operation_class = GEGL_OPERATION_CLASS (klass);
  GeglOperationMetaClass *operation_meta_class = GEGL_OPERATION_META_CLASS (klass);

  operation_class->attach = attach;
  operation_meta_class->update = update_graph;
 /*btw, if Graphical Effects (or any of my plugins) ever breaks try changing the name space from gegl: or lb: to something else.*/
  gegl_operation_class_set_keys (operation_class,
    "name",        "lb:sharpen-deluxe",
    "title",       _("Sharpen Deluxe"),
    "reference-hash", "535233dashacpen",
    "description", _("This filter grain extracts blurs and anisotropics to make a high pass. The gray outlined image is a high pass and it is suppose to be blended by Gimp's 'Blending Options/Blend Modes'. Grain Merge is a true sharpen, but experiment with other blend modes and the opacity slider. The user can also use the auto grain merge checkbox for convince. "
                     ""),
    "gimp:menu-path", "<Image>/Filters/Enhance",
    "gimp:menu-label", _("Sharpen Deluxe..."),
/*This only appears in the menu path if used in Gimp 2.99.16 and up. It will not be in 2.10's menu unless Gimp's team backports, Aug 19 2023*/
    NULL);
}

#endif
