//  Copyright (c) 2007, Pavel Ledin
//
//  All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are
//  met:
//  *       Redistributions of source code must retain the above copyright
//  notice, this list of conditions and the following disclaimer.
//  *       Redistributions in binary form must reproduce the above
//  copyright notice, this list of conditions and the following disclaimer
//  in the documentation and/or other materials provided with the
//  distribution.
//  *       Neither the name of Pavel Ledin nor the names of
//  its other contributors may be used to endorse or promote products derived
//  from this software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
//  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
//  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "shader.h"
#include "geoshader.h"

const int MAX_PATH_L = 512;

typedef struct
{
	miBoolean useBuffers;
	miTag fname;
	miTag render_dir;
	int frame_padding;
	int output_format;
	int openexr_comp;
	miBoolean filter_pass;
	miBoolean contrast_all_buffers;
	
	//ambien occlusion
	miBoolean gpuAO;
//	int aoGpuPasses;
//	int aoRays;
	miScalar aoFalloff;
	miScalar aoMinDistance;
	miScalar aoMaxDistance;
	
	//mila string options
	miBoolean milaClamp;
	
	//int fb_virtual;
	miBoolean	overrideFbMemManagement;
	miInteger	fbMemManagement;
	miBoolean mr_color_pass;
	int mr_color_format;
	miBoolean mr_z_pass;
	int mr_z_format;
	miBoolean mr_normal_pass;
	int mr_normal_format;
	miBoolean mr_motion_pass;
	int mr_motion_format;
	miBoolean mr_label_pass;
	int mr_label_format;
	
	miBoolean direct_diffuse_pass;
	miBoolean indirect_diffuse_pass;
	miBoolean direct_glossy_pass;
	miBoolean indirect_glossy_pass;
	miBoolean direct_specular_pass;
	miBoolean indirect_specular_pass;
	miBoolean diffuse_transmission_pass;
	miBoolean glossy_transmission_pass;
	miBoolean specular_transmission_pass;
	miBoolean front_scatter_pass;
	miBoolean back_scatter_pass;
	miBoolean emission_pass;
	miBoolean gpuAO_pass;
	miBoolean UserPass1_pass;
	miTag 	  UserPass1_str;
	miBoolean UserPass2_pass;
	miTag 	  UserPass2_str;
	miBoolean UserPass3_pass;
	miTag 	  UserPass3_str;
	miBoolean UserPass4_pass;
	miTag 	  UserPass4_str;
	miBoolean modifyCamera;
    miInteger mode; // 0=off, 1=toein, 2=offaxis, 3=offset
    miScalar eyeDist;	
} mla_Buffers_pass_t;

int get_frame_number(miState* state, char *padd, int pad)
{
	int frame_n = state->camera->frame;
//	sprintf(s, "%04d", i)
//	sprintf(buffer, "%i", value);
//	std::string www;
	double int_padding = 10;
	if(frame_n != 0)
	{
		double float_padding = 1000000000;
		while((int)float_padding > frame_n)
		{
			int_padding--;
			float_padding /= 10;
		}
		if(int_padding > pad)
			pad = (int)int_padding;
	}
	else
		int_padding = 1;

	sprintf(padd, "%d", frame_n);

	char result[11];
	for(int i = 0; i < 11; i++)
		result[i] = 0;

	for(int i = (int)int_padding; i < pad; i++)
		strcat(result, "0");

	strcat(result, padd);
	strcpy(padd, result);
	return 1;
}

int get_render_file_func(miState* state, char *dir, char *name, char *prefix, int pad, char *ext)
{
	char frame_num[11];
	get_frame_number(state, frame_num, pad);
	strcat(dir, "/");				//render_dir + "/"
	strcat(dir, name);				//add name
	strcat(dir, "-");				//add minus
	strcat(dir, prefix);			//add prefix
	strcat(dir, "_");				//add underline
	strcat(dir, frame_num);			//add frame number
	strcat(dir, ".");				//add dot
	strcat(dir, ext);				//add format
	return 1;
}

char * get_type(int out_type)
{
	char *type;
	switch(out_type)
	{
		case 0:				// 0, mean float point TIF
			type = mi_mem_strdup("rgba_16");
			break;
		case 1:				// 1, mean 32bit IFF
			type = mi_mem_strdup("rgba_fp");
			break;
		case 2:				// 2, mean OpenEXR (mi) single file
			type = mi_mem_strdup("rgba_fp");
			break;
		case 3:				// 3, mean OpenEXR (mi) multiple files
			type = mi_mem_strdup("rgba_fp");
			break;
		case 4:				// 4, mean png
			type = mi_mem_strdup("rgba");
			break;	
		default:				//default, float point TIF
			type = mi_mem_strdup("rgba_16");
			break;
	}
	return type;
}

char * get_ext(int out_ext)
{
	char *ext;
	switch(out_ext)
	{
		case 0:				// 0, mean float point TIF
			ext = mi_mem_strdup("tif");
			break;
		case 1:				// 1, mean 32bit IFF
			ext = mi_mem_strdup("iff");
			break;
		case 2:				// 2, mean OpenEXR (mi) single file
			ext = mi_mem_strdup("exr");
			break;
		case 3:				// 3, mean OpenEXR multiple files
			ext = mi_mem_strdup("exr");
			break;
		case 4:				// 4, mean png
			ext = mi_mem_strdup("png");
			break;	
		default:				//default, float point TIF
			ext = mi_mem_strdup("tif");
			break;
	}
	return ext;
}


miTag create_output_shaders(char *buffer_a, char *buffer_b, int operation)
{
	miTag result = 0;
	if(mi_api_function_call(mi_mem_strdup("p_math_output")))
	{
		
		if(mi_api_parameter_name(mi_mem_strdup("buffer_a")))			//buffer_a
		{
			int param_size = sizeof(buffer_a);
			mi_api_parameter_value(miTYPE_STRING, buffer_a, NULL, &param_size);
		}
		else
		{
			mi_warning("p_MegaTK_pass: buffer_a parameter not created in function p_math_output");
			return result;
		}

		if(mi_api_parameter_name(mi_mem_strdup("buffer_b")))			//buffer_b
		{
			int param_size = sizeof(buffer_b);
			mi_api_parameter_value(miTYPE_STRING, buffer_b, NULL, &param_size);
		}
		else
		{
			mi_warning("p_MegaTK_pass: buffer_b parameter not created in function p_math_output");
			return result;
		}


		if(mi_api_parameter_name(mi_mem_strdup("operation")))			//operation
		{
			int *default_value = &operation;
			int param_size = sizeof(miInteger);
//			mi_api_parameter_default(miTYPE_INTEGER, default_value);
			mi_api_parameter_value(miTYPE_INTEGER, default_value, NULL, &param_size);
		}
		else
		{
			mi_warning("p_MegaTK_pass: operation parameter not created in function p_math_output");
			return result;
		}

		miTag oldtag = 0;
		result = mi_api_function_call_end(oldtag);
		return result;
	}
	else
	{
		mi_warning("p_MegaTK_pass: function p_math_output not found");
	}	
	return result;
}

miTag declare_output_shaders(void)
{
	miTag new_func_tag;
	//declare function
	miTag check_tag = mi_api_name_lookup(mi_mem_strdup("p_math_output"));
	if(check_tag == miNULLTAG)	//function not declared yet, declare it
	{
		miParameter *inputParamet;
		inputParamet = mi_api_parameter_decl(miTYPE_STRING, mi_mem_strdup("buffer_a"), 0);
		inputParamet = mi_api_parameter_append(inputParamet, mi_api_parameter_decl(miTYPE_STRING, mi_mem_strdup("buffer_b"), 0));
		inputParamet = mi_api_parameter_append(inputParamet, mi_api_parameter_decl(miTYPE_INTEGER, mi_mem_strdup("operation"), 0));

		miParameter *outParamet;
		outParamet = mi_api_parameter_decl(miTYPE_BOOLEAN, mi_mem_strdup("result"), 0);
		miFunction_decl *func_dec;
		func_dec = mi_api_funcdecl_begin(outParamet, mi_mem_strdup("p_math_output"), inputParamet);
		func_dec->version = 2;
		new_func_tag = mi_api_funcdecl_end();
	}
	return new_func_tag;
}


extern "C"
{

DLLEXPORT int mla_Buffers_pass_version(void) 
{
	return(1);
}

DLLEXPORT miBoolean mla_Buffers_pass(miTag *result, miState *state, mla_Buffers_pass_t *paras)
{		
	if( *mi_eval_boolean( &paras->useBuffers ) ) {
	
	miTag name_tag = *mi_eval_tag(&paras->fname);				//file name
	if (!name_tag)
		return miFALSE;
	char *name = (char*)mi_db_access(name_tag);
	mi_db_unpin(name_tag);

	miTag render_dir_tag = *mi_eval_tag(&paras->render_dir);	//render dir
	if (!render_dir_tag)
		return miFALSE;
	char *render_dir = (char*)mi_db_access(render_dir_tag);
	mi_db_unpin(render_dir_tag);

	int frame_padding = *mi_eval_integer(&paras->frame_padding);

	miBoolean filtering = *mi_eval_boolean(&paras->filter_pass);
	miBoolean contrast_all = *mi_eval_boolean(&paras->contrast_all_buffers);
	
//	int ao_gpu_passes = *mi_eval_integer(&paras->aoGpuPasses);
//	int ao_rays = *mi_eval_integer(&paras->aoRays);
	miScalar ao_falloff = *mi_eval_scalar(&paras->aoFalloff);
	miScalar ao_min_distance = *mi_eval_scalar(&paras->aoMinDistance);
	miScalar ao_max_distance = *mi_eval_scalar(&paras->aoMaxDistance);

	miCamera *camera = (miCamera *)state->camera;

	miUint typemap = miIMG_TYPE_RGB_FP;
	miUint interpmap = miIMG_TYPE_RGB_FP;

	int out_format = *mi_eval_integer(&paras->output_format);
	int openxer_comp = *mi_eval_integer(&paras->openexr_comp);

	switch(out_format)
	{
		case 0:				// 0, mean float point TIF
			typemap = miIMG_TYPE_RGBA_16;
			interpmap = miIMG_TYPE_RGBA_16;
			break;
		case 1:				// 1, mean 32bit IFF
			typemap = miIMG_TYPE_RGBA_FP;
			interpmap = miIMG_TYPE_RGBA_FP;
			break;
		case 2:				// 2, mean OpenEXR (mi) single file
			typemap = miIMG_TYPE_RGBA_FP;
			interpmap = miIMG_TYPE_RGBA_FP;
			break;
		case 3:				// 3, mean OpenEXR multiple files
			typemap = miIMG_TYPE_RGBA_FP;
			interpmap = miIMG_TYPE_RGBA_FP;
			break;
		case 4:				// 4, mean png
			typemap = miIMG_TYPE_RGBA;
			interpmap = miIMG_TYPE_RGBA;
			break;	
		default:				//default, float point TIF
			typemap = miIMG_TYPE_RGBA_16;
			interpmap = miIMG_TYPE_RGBA_16;
			break;
	}

	char* exr_comp;
	switch(openxer_comp)
	{
		case 0:
			exr_comp = "rle";
			break;
		case 1:
			exr_comp = "zip";
			break;
		case 2:
			exr_comp = "piz";
			break;
		case 3:
			exr_comp = "pxr24";
			break;
		case 4:
			exr_comp = "none";
			break;
		default:
			exr_comp = "rle";
			break;
	}

	//set fb_virtual option
	if(*mi_eval_integer(&paras->overrideFbMemManagement)) {
		int fb_management = *mi_eval_integer(&paras->fbMemManagement);

		miOptions *options = mi_api_options_begin(mi_mem_strdup("miDefaultOptions"));
		options->fb_virtual = fb_management;
		mi_api_options_end();
	}
	
	
	if(1)    //enable contrast all buffers
	{
		mi::shader::Interface *iface = mi_get_shader_interface();
		mi::shader::Options *options = iface->getOptions(state->options->string_options);
		iface->release();
		if(contrast_all)
			options->set("contrast all buffers", true);
		else
			options->set("contrast all buffers", false);
		options->release();
	}


	if(1)    //enable buffers in trasparency for rasterizer
	{
		mi::shader::Interface *iface = mi_get_shader_interface();
		mi::shader::Options *options = iface->getOptions(state->options->string_options);
		iface->release();
//		options->set("useopacity", true);
		options->set("rast useopacity", true);
		options->release();
	}
	
	// ambient occlusion
	if( *mi_eval_boolean(&paras->gpuAO) )
	{		
		mi::shader::Interface *iface = mi_get_shader_interface();
		mi::shader::Options *options = iface->getOptions(state->options->string_options);
		iface->release();
		options->set("ambient occlusion gpu", true);
	//	options->set("ambient occlusion cache", ao_gpu_passes);
	//	options->set("ambient occlusion rays", ao_rays);
		options->set("ambient occlusion falloff", ao_falloff);
		options->set("ambient occlusion falloff min distance", ao_min_distance);
		options->set("ambient occlusion max distance", ao_max_distance);
		options->set("ambient occlusion framebuffer", "gpuAO");		
		options->release();		
	} 
	
	if( *mi_eval_boolean(&paras->milaClamp) )
	{		
		mi::shader::Interface *iface = mi_get_shader_interface();
		mi::shader::Options *options = iface->getOptions(state->options->string_options);
		iface->release();
		options->set("mila clamp output", true);	
		options->release();		
	} 

/////////////////////////////// Call Output Shaders
//	mi_api_incremental(1);
	mi_api_function_delete(&camera->output); //delete all output shaders

//	mi_api_incremental(0);

	///////////////////////////// Define Buffers

	mi::shader::Edit_fb fbc(state->camera->buffertag);
//	fbc->reset();
	
	if(*mi_eval_boolean(&paras->mr_color_pass))
	{
		char *c_format;
		switch(*mi_eval_integer(&paras->mr_color_format))
		{
			case 0:
				c_format = mi_mem_strdup("tif");
				break;
			case 1:
				c_format = mi_mem_strdup("iff");
				break;
			case 2:
				c_format = mi_mem_strdup("exr");
				break;
			case 3:
				c_format = mi_mem_strdup("exr");
				break;
			case 4:
				c_format = mi_mem_strdup("png");
				break;	
			default:
				c_format = mi_mem_strdup("tif");
				break;
		}
		char *ctype;
		switch(*mi_eval_integer(&paras->mr_color_format))
		{
			case 0:				// 0, mean float point TIF
				ctype = mi_mem_strdup("rgba_16");
				break;
			case 1:				// 1, mean 32bit IFF
				ctype = mi_mem_strdup("rgba_fp");
				break;
			case 2:				// 2, mean OpenEXR (mi) single file
				ctype = mi_mem_strdup("rgba_fp");
				break;
			case 3:				// 3, mean OpenEXR (mi) multiple files
				ctype = mi_mem_strdup("rgba_fp");
				break;
			case 4:				// 4, mean png
				ctype = mi_mem_strdup("rgba");
				break;	
			default:				//default, float point TIF
				ctype = mi_mem_strdup("rgba_16");
				break;
		}
		char render_dir_cur_pass[MAX_PATH_L];
		strcpy(render_dir_cur_pass, render_dir);

			if(*mi_eval_integer(&paras->mr_color_format) != 2)
				get_render_file_func(state, render_dir_cur_pass, name, "color", frame_padding, c_format);
			else 
			{
				get_render_file_func(state, render_dir_cur_pass, name, "single", frame_padding, get_ext(out_format));
			}
			if (*mi_eval_integer(&paras->mr_color_format) == 2 || *mi_eval_integer(&paras->mr_color_format) == 3) {
				fbc->set("color", "compression", exr_comp);	
			}
		fbc->set("color", "filename", render_dir_cur_pass);
		fbc->set("color", "filetype", c_format);
		fbc->set("color", "datatype", ctype);
		fbc->set("color", "user", false);
		fbc->set("color", "primary", true);
		if(filtering == miTRUE)
			fbc->set("color", "filtering", true);
		else
			fbc->set("color", "filtering", false);
	}

	if(*mi_eval_boolean(&paras->mr_z_pass))
	{
		char *z_format;
		switch(*mi_eval_integer(&paras->mr_z_format))
		{
			case 0:
				z_format = mi_mem_strdup("zt");
				break;
			case 1:
				z_format = mi_mem_strdup("tif");
				break;
			case 2:
				z_format = mi_mem_strdup("exr");
				break;
			case 3:
				z_format = mi_mem_strdup("exr");
				break;
			default:
				z_format = mi_mem_strdup("zt");
				break;
		}		
		char render_dir_cur_pass[MAX_PATH_L];
		strcpy(render_dir_cur_pass, render_dir);

			if(*mi_eval_integer(&paras->mr_z_format) != 2)
				get_render_file_func(state, render_dir_cur_pass, name, "zdepth", frame_padding, z_format);
			else 
			{
				get_render_file_func(state, render_dir_cur_pass, name, "single", frame_padding, get_ext(out_format));
				fbc->set("Z", "compression", exr_comp);
			}
			fbc->set("Z", "filename", render_dir_cur_pass);
			fbc->set("Z", "filetype", z_format);
		fbc->set("Z", "datatype", "z");
		fbc->set("Z", "user", false);
		if(filtering == miTRUE)
			fbc->set("Z", "filtering", true);
		else
			fbc->set("Z", "filtering", false);
	}

	if(*mi_eval_boolean(&paras->mr_normal_pass))
	{
		char *n_format;
		switch(*mi_eval_integer(&paras->mr_normal_format))
		{
			case 0:
				n_format = mi_mem_strdup("nt");
				break;
			case 1:
				n_format = mi_mem_strdup("tif");
				break;
			case 2:
				n_format = mi_mem_strdup("exr");
				break;
			case 3:
				n_format = mi_mem_strdup("exr");
				break;
			default:
				n_format = mi_mem_strdup("nt");
				break;
		}
		char render_dir_cur_pass[MAX_PATH_L];
		strcpy(render_dir_cur_pass, render_dir);

			if(*mi_eval_integer(&paras->mr_normal_format) != 2)
				get_render_file_func(state, render_dir_cur_pass, name, "Normal", frame_padding, n_format);
			else
			{
				get_render_file_func(state, render_dir_cur_pass, name, "single", frame_padding, get_ext(out_format));
				fbc->set("Normal", "compression", exr_comp);
			}
			fbc->set("Normal", "filename", render_dir_cur_pass);
			fbc->set("Normal", "filetype", n_format);
		fbc->set("Normal", "datatype", "n");
		fbc->set("Normal", "user", false);
		if(filtering == miTRUE)
			fbc->set("Normal", "filtering", true);
		else
			fbc->set("Normal", "filtering", false);
	}

	if(*mi_eval_boolean(&paras->mr_motion_pass))
	{
		char *m_format;
		switch(*mi_eval_integer(&paras->mr_motion_format))
		{
			case 0:
				m_format = mi_mem_strdup("mt");
				break;
			case 1:
				m_format = mi_mem_strdup("tif");
				break;
			case 2:
				m_format = mi_mem_strdup("exr");
				break;
			case 3:
				m_format = mi_mem_strdup("exr");
				break;
			default:
				m_format = mi_mem_strdup("mt");
				break;
		}
		char render_dir_cur_pass[MAX_PATH_L];
		strcpy(render_dir_cur_pass, render_dir);

			if(*mi_eval_integer(&paras->mr_motion_format) != 2)
				get_render_file_func(state, render_dir_cur_pass, name, "Motion", frame_padding, m_format);
			else
			{
				get_render_file_func(state, render_dir_cur_pass, name, "single", frame_padding, get_ext(out_format));
				fbc->set("Motion", "compression", exr_comp);
			}
			fbc->set("Motion", "filename", render_dir_cur_pass);
			fbc->set("Motion", "filetype", m_format);
		fbc->set("Motion", "datatype", "m");
		fbc->set("Motion", "user", false);
		if(filtering == miTRUE)
			fbc->set("Motion", "filtering", true);
		else
			fbc->set("Motion", "filtering", false);
	}

	if(*mi_eval_boolean(&paras->mr_label_pass))
	{
		char *label_format;
		switch(*mi_eval_integer(&paras->mr_label_format))
		{
			case 0:
				label_format = mi_mem_strdup("tt");
				break;
			case 1:
				label_format = mi_mem_strdup("tif");
				break;
			case 2:
				label_format = mi_mem_strdup("exr");
				break;
			case 3:
				label_format = mi_mem_strdup("exr");
				break;
			default:
				label_format = mi_mem_strdup("tt");
				break;
		}
		char render_dir_cur_pass[MAX_PATH_L];
		strcpy(render_dir_cur_pass, render_dir);
			if(*mi_eval_integer(&paras->mr_label_format) != 2)
				get_render_file_func(state, render_dir_cur_pass, name, "Label", frame_padding, label_format);
			else
			{
				get_render_file_func(state, render_dir_cur_pass, name, "single", frame_padding, get_ext(out_format));
				fbc->set("Label", "compression", exr_comp);
			}
			fbc->set("Label", "filename", render_dir_cur_pass);
			fbc->set("Label", "filetype", label_format);
		fbc->set("Label", "datatype", "tag");
		fbc->set("Label", "user", false);
	}

	if(*mi_eval_boolean(&paras->direct_diffuse_pass))									//direct_diffuse pass
	{
		char render_dir_cur_pass[MAX_PATH_L];
		strcpy(render_dir_cur_pass, render_dir);

			if(out_format != 2) {
				get_render_file_func(state, render_dir_cur_pass, name, "direct_diffuse", frame_padding, get_ext(out_format));
			} else {
				get_render_file_func(state, render_dir_cur_pass, name, "single", frame_padding, get_ext(out_format));
				}
			if (out_format == 2 || out_format == 3) {
				fbc->set("direct_diffuse", "compression", exr_comp);	
			}		
		fbc->set("direct_diffuse", "filename", render_dir_cur_pass);
		fbc->set("direct_diffuse", "filetype", get_ext(out_format));
		fbc->set("direct_diffuse", "datatype", get_type(out_format));
		fbc->set("direct_diffuse", "user", true);
		fbc->set("direct_diffuse", "useopacity", true);
		if(filtering == miTRUE)
			fbc->set("direct_diffuse", "filtering", true);
		else
			fbc->set("direct_diffuse", "filtering", false);
	}
	
	if(*mi_eval_boolean(&paras->indirect_diffuse_pass))									//indirect_diffuse pass
	{
		char render_dir_cur_pass[MAX_PATH_L];
		strcpy(render_dir_cur_pass, render_dir);

			if(out_format != 2) {
				get_render_file_func(state, render_dir_cur_pass, name, "indirect_diffuse", frame_padding, get_ext(out_format));
			} else {
				get_render_file_func(state, render_dir_cur_pass, name, "single", frame_padding, get_ext(out_format));
				}
			if (out_format == 2 || out_format == 3) {
				fbc->set("indirect_diffuse", "compression", exr_comp);	
			}	
		fbc->set("indirect_diffuse", "filename", render_dir_cur_pass);
		fbc->set("indirect_diffuse", "filetype", get_ext(out_format));
		fbc->set("indirect_diffuse", "datatype", get_type(out_format));
		fbc->set("indirect_diffuse", "user", true);
		fbc->set("indirect_diffuse", "useopacity", true);
		if(filtering == miTRUE)
			fbc->set("indirect_diffuse", "filtering", true);
		else
			fbc->set("indirect_diffuse", "filtering", false);
	}
	
	if(*mi_eval_boolean(&paras->direct_glossy_pass))									//direct_glossy pass
	{
		char render_dir_cur_pass[MAX_PATH_L];
		strcpy(render_dir_cur_pass, render_dir);
		
			if(out_format != 2) {
				get_render_file_func(state, render_dir_cur_pass, name, "direct_glossy", frame_padding, get_ext(out_format));
			} else {
				get_render_file_func(state, render_dir_cur_pass, name, "single", frame_padding, get_ext(out_format));
				}
			if (out_format == 2 || out_format == 3) {
				fbc->set("direct_glossy", "compression", exr_comp);	
			}		
		fbc->set("direct_glossy", "filename", render_dir_cur_pass);
		fbc->set("direct_glossy", "filetype", get_ext(out_format));
		fbc->set("direct_glossy", "datatype", get_type(out_format));
		fbc->set("direct_glossy", "user", true);
		fbc->set("direct_glossy", "useopacity", true);
		if(filtering == miTRUE)
			fbc->set("direct_glossy", "filtering", true);
		else
			fbc->set("direct_glossy", "filtering", false);
	}
	
	if(*mi_eval_boolean(&paras->indirect_glossy_pass))									//indirect_glossy pass
	{
		char render_dir_cur_pass[MAX_PATH_L];
		strcpy(render_dir_cur_pass, render_dir);

			if(out_format != 2) {
				get_render_file_func(state, render_dir_cur_pass, name, "indirect_glossy", frame_padding, get_ext(out_format));
			} else {
				get_render_file_func(state, render_dir_cur_pass, name, "single", frame_padding, get_ext(out_format));
				}
			if (out_format == 2 || out_format == 3) {
				fbc->set("indirect_glossy", "compression", exr_comp);	
			}		
		fbc->set("indirect_glossy", "filename", render_dir_cur_pass);
		fbc->set("indirect_glossy", "filetype", get_ext(out_format));
		fbc->set("indirect_glossy", "datatype", get_type(out_format));
		fbc->set("indirect_glossy", "user", true);
		fbc->set("indirect_glossy", "useopacity", true);
		if(filtering == miTRUE)
			fbc->set("indirect_glossy", "filtering", true);
		else
			fbc->set("indirect_glossy", "filtering", false);
	}
	
	if(*mi_eval_boolean(&paras->direct_specular_pass))									//direct_specular pass
	{
		char render_dir_cur_pass[MAX_PATH_L];
		strcpy(render_dir_cur_pass, render_dir);

			if(out_format != 2) {
				get_render_file_func(state, render_dir_cur_pass, name, "direct_specular", frame_padding, get_ext(out_format));
			} else {
				get_render_file_func(state, render_dir_cur_pass, name, "single", frame_padding, get_ext(out_format));
				}
			if (out_format == 2 || out_format == 3) {
				fbc->set("direct_specular", "compression", exr_comp);	
			}	
		fbc->set("direct_specular", "filename", render_dir_cur_pass);
		fbc->set("direct_specular", "filetype", get_ext(out_format));
		fbc->set("direct_specular", "datatype", get_type(out_format));
		fbc->set("direct_specular", "user", true);
		fbc->set("direct_specular", "useopacity", true);
		if(filtering == miTRUE)
			fbc->set("direct_specular", "filtering", true);
		else
			fbc->set("direct_specular", "filtering", false);
	}
	
	if(*mi_eval_boolean(&paras->indirect_specular_pass))									//indirect_specular pass
	{
		char render_dir_cur_pass[MAX_PATH_L];
		strcpy(render_dir_cur_pass, render_dir);

			if(out_format != 2) {
				get_render_file_func(state, render_dir_cur_pass, name, "indirect_specular", frame_padding, get_ext(out_format));
			} else {
				get_render_file_func(state, render_dir_cur_pass, name, "single", frame_padding, get_ext(out_format));
				}
			if (out_format == 2 || out_format == 3) {
				fbc->set("indirect_specular", "compression", exr_comp);	
			}		
		fbc->set("indirect_specular", "filename", render_dir_cur_pass);
		fbc->set("indirect_specular", "filetype", get_ext(out_format));
		fbc->set("indirect_specular", "datatype", get_type(out_format));
		fbc->set("indirect_specular", "user", true);
		fbc->set("indirect_specular", "useopacity", true);
		if(filtering == miTRUE)
			fbc->set("indirect_specular", "filtering", true);
		else
			fbc->set("indirect_specular", "filtering", false);
	}
	
	if(*mi_eval_boolean(&paras->diffuse_transmission_pass))									//diffuse_transmission pass
	{
		char render_dir_cur_pass[MAX_PATH_L];
		strcpy(render_dir_cur_pass, render_dir);

			if(out_format != 2) {
				get_render_file_func(state, render_dir_cur_pass, name, "diffuse_transmission", frame_padding, get_ext(out_format));
			} else {
				get_render_file_func(state, render_dir_cur_pass, name, "single", frame_padding, get_ext(out_format));
				}
			if (out_format == 2 || out_format == 3) {
				fbc->set("diffuse_transmission", "compression", exr_comp);	
			}	
		fbc->set("diffuse_transmission", "filename", render_dir_cur_pass);
		fbc->set("diffuse_transmission", "filetype", get_ext(out_format));
		fbc->set("diffuse_transmission", "datatype", get_type(out_format));
		fbc->set("diffuse_transmission", "user", true);
		fbc->set("diffuse_transmission", "useopacity", true);
		if(filtering == miTRUE)
			fbc->set("diffuse_transmission", "filtering", true);
		else
			fbc->set("diffuse_transmission", "filtering", false);
	}
	
	if(*mi_eval_boolean(&paras->glossy_transmission_pass))									//glossy_transmission pass
	{
		char render_dir_cur_pass[MAX_PATH_L];
		strcpy(render_dir_cur_pass, render_dir);

			if(out_format != 2) {
				get_render_file_func(state, render_dir_cur_pass, name, "glossy_transmission", frame_padding, get_ext(out_format));
			} else {
				get_render_file_func(state, render_dir_cur_pass, name, "single", frame_padding, get_ext(out_format));
				}
			if (out_format == 2 || out_format == 3) {
				fbc->set("glossy_transmission", "compression", exr_comp);	
			}		
		fbc->set("glossy_transmission", "filename", render_dir_cur_pass);
		fbc->set("glossy_transmission", "filetype", get_ext(out_format));
		fbc->set("glossy_transmission", "datatype", get_type(out_format));
		fbc->set("glossy_transmission", "user", true);
		fbc->set("glossy_transmission", "useopacity", true);
		if(filtering == miTRUE)
			fbc->set("glossy_transmission", "filtering", true);
		else
			fbc->set("glossy_transmission", "filtering", false);
	}
	
	if(*mi_eval_boolean(&paras->specular_transmission_pass))									//specular_transmission pass
	{
		char render_dir_cur_pass[MAX_PATH_L];
		strcpy(render_dir_cur_pass, render_dir);

			if(out_format != 2) {
				get_render_file_func(state, render_dir_cur_pass, name, "specular_transmission", frame_padding, get_ext(out_format));
			} else { 
				get_render_file_func(state, render_dir_cur_pass, name, "single", frame_padding, get_ext(out_format));
				}
			if (out_format == 2 || out_format == 3) {
				fbc->set("specular_transmission", "compression", exr_comp);	
			}	
		fbc->set("specular_transmission", "filename", render_dir_cur_pass);
		fbc->set("specular_transmission", "filetype", get_ext(out_format));	
		fbc->set("specular_transmission", "datatype", get_type(out_format));
		fbc->set("specular_transmission", "user", true);
		fbc->set("specular_transmission", "useopacity", true);
		if(filtering == miTRUE)
			fbc->set("specular_transmission", "filtering", true);
		else
			fbc->set("specular_transmission", "filtering", false);
	}
	
	if(*mi_eval_boolean(&paras->front_scatter_pass))									//front_scatter pass
	{
		char render_dir_cur_pass[MAX_PATH_L];
		strcpy(render_dir_cur_pass, render_dir);

			if(out_format != 2) {
				get_render_file_func(state, render_dir_cur_pass, name, "front_scatter", frame_padding, get_ext(out_format));
			 } else {
				get_render_file_func(state, render_dir_cur_pass, name, "single", frame_padding, get_ext(out_format));
				}
			if (out_format == 2 || out_format == 3) {
				fbc->set("front_scatter", "compression", exr_comp);	
			}		
		fbc->set("front_scatter", "filename", render_dir_cur_pass);
		fbc->set("front_scatter", "filetype", get_ext(out_format));
		fbc->set("front_scatter", "datatype", get_type(out_format));
		fbc->set("front_scatter", "user", true);
		fbc->set("front_scatter", "useopacity", true);
		if(filtering == miTRUE)
			fbc->set("front_scatter", "filtering", true);
		else
			fbc->set("front_scatter", "filtering", false);
	}
	
	if(*mi_eval_boolean(&paras->back_scatter_pass))									//back_scatter pass
	{
		char render_dir_cur_pass[MAX_PATH_L];
		strcpy(render_dir_cur_pass, render_dir);
		
			if(out_format != 2) {
				get_render_file_func(state, render_dir_cur_pass, name, "back_scatter", frame_padding, get_ext(out_format));
			} else {
				get_render_file_func(state, render_dir_cur_pass, name, "single", frame_padding, get_ext(out_format));
				}
			if (out_format == 2 || out_format == 3) {
				fbc->set("back_scatter", "compression", exr_comp);	
			}		
		fbc->set("back_scatter", "filename", render_dir_cur_pass);
		fbc->set("back_scatter", "filetype", get_ext(out_format));
		fbc->set("back_scatter", "datatype", get_type(out_format));
		fbc->set("back_scatter", "user", true);
		fbc->set("back_scatter", "useopacity", true);
		if(filtering == miTRUE)
			fbc->set("back_scatter", "filtering", true);
		else
			fbc->set("back_scatter", "filtering", false);
	}
	
	if(*mi_eval_boolean(&paras->emission_pass))									//emission pass
	{
		char render_dir_cur_pass[MAX_PATH_L];
		strcpy(render_dir_cur_pass, render_dir);

			if(out_format != 2) {
				get_render_file_func(state, render_dir_cur_pass, name, "emission", frame_padding, get_ext(out_format));
			} else {
				get_render_file_func(state, render_dir_cur_pass, name, "single", frame_padding, get_ext(out_format));
				}
			if (out_format == 2 || out_format == 3) {
				fbc->set("emission", "compression", exr_comp);	
			}	
		fbc->set("emission", "filename", render_dir_cur_pass);
		fbc->set("emission", "filetype", get_ext(out_format));
		fbc->set("emission", "datatype", get_type(out_format));
		fbc->set("emission", "user", true);
		fbc->set("emission", "useopacity", true);
		if(filtering == miTRUE)
			fbc->set("emission", "filtering", true);
		else
			fbc->set("emission", "filtering", false);
	}
	
	if(*mi_eval_boolean(&paras->gpuAO_pass))									//gpuAO pass
	{
		char render_dir_cur_pass[MAX_PATH_L];
		strcpy(render_dir_cur_pass, render_dir);

			if(out_format != 2) {
				get_render_file_func(state, render_dir_cur_pass, name, "gpuAO", frame_padding, get_ext(out_format));
			} else {
				get_render_file_func(state, render_dir_cur_pass, name, "single", frame_padding, get_ext(out_format));
				}
			if (out_format == 2 || out_format == 3) {
				fbc->set("gpuAO", "compression", exr_comp);	
			}	
		fbc->set("gpuAO", "filename", render_dir_cur_pass);
		fbc->set("gpuAO", "filetype", get_ext(out_format));
		fbc->set("gpuAO", "datatype", get_type(out_format));
		fbc->set("gpuAO", "user", true);
		fbc->set("gpuAO", "useopacity", true);
		if(filtering == miTRUE)
			fbc->set("gpuAO", "filtering", true);
		else
			fbc->set("gpuAO", "filtering", false);
	}
	
	if(*mi_eval_boolean(&paras->UserPass1_pass))									//UserPass1 pass
	{
		miTag UserPass1_Name = *mi_eval_tag(&paras->UserPass1_str);
		if (!UserPass1_Name)
			return miFALSE;
		char *Pass1_Name = (char*)mi_db_access(UserPass1_Name);
		mi_db_unpin(UserPass1_Name);
		
		char render_dir_cur_pass[MAX_PATH_L];
		strcpy(render_dir_cur_pass, render_dir);

			if(out_format != 2) {
				get_render_file_func(state, render_dir_cur_pass, name, Pass1_Name, frame_padding, get_ext(out_format));
			} else {
				get_render_file_func(state, render_dir_cur_pass, name, "single", frame_padding, get_ext(out_format));
				}
			if (out_format == 2 || out_format == 3) {
				fbc->set(Pass1_Name, "compression", exr_comp);	
			}	
		fbc->set(Pass1_Name, "filename", render_dir_cur_pass);
		fbc->set(Pass1_Name, "filetype", get_ext(out_format));
		fbc->set(Pass1_Name, "datatype", get_type(out_format));
		fbc->set(Pass1_Name, "user", true);
		fbc->set(Pass1_Name, "useopacity", true);
		if(filtering == miTRUE)
			fbc->set(Pass1_Name, "filtering", true);
		else
			fbc->set(Pass1_Name, "filtering", false);
	}
	
	if(*mi_eval_boolean(&paras->UserPass2_pass))									//UserPass2 pass
	{
		miTag UserPass2_Name = *mi_eval_tag(&paras->UserPass2_str);
		if (!UserPass2_Name)
			return miFALSE;
		char *Pass2_Name = (char*)mi_db_access(UserPass2_Name);
		mi_db_unpin(UserPass2_Name);
		
		char render_dir_cur_pass[MAX_PATH_L];
		strcpy(render_dir_cur_pass, render_dir);

			if(out_format != 2) {
				get_render_file_func(state, render_dir_cur_pass, name, Pass2_Name, frame_padding, get_ext(out_format));
			} else {
				get_render_file_func(state, render_dir_cur_pass, name, "single", frame_padding, get_ext(out_format));
				}
			if (out_format == 2 || out_format == 3) {
				fbc->set(Pass2_Name, "compression", exr_comp);	
			}		
		fbc->set(Pass2_Name, "filename", render_dir_cur_pass);
		fbc->set(Pass2_Name, "filetype", get_ext(out_format));
		fbc->set(Pass2_Name, "datatype", get_type(out_format));
		fbc->set(Pass2_Name, "user", true);
		fbc->set(Pass2_Name, "useopacity", true);
		if(filtering == miTRUE)
			fbc->set(Pass2_Name, "filtering", true);
		else
			fbc->set(Pass2_Name, "filtering", false);
	}
	
	if(*mi_eval_boolean(&paras->UserPass3_pass))									//UserPass3 pass
	{
		miTag UserPass3_Name = *mi_eval_tag(&paras->UserPass3_str);
		if (!UserPass3_Name)
			return miFALSE;
		char *Pass3_Name = (char*)mi_db_access(UserPass3_Name);
		mi_db_unpin(UserPass3_Name);
		
		char render_dir_cur_pass[MAX_PATH_L];
		strcpy(render_dir_cur_pass, render_dir);

			if(out_format != 2) {
				get_render_file_func(state, render_dir_cur_pass, name, Pass3_Name, frame_padding, get_ext(out_format));
			} else {
				get_render_file_func(state, render_dir_cur_pass, name, "single", frame_padding, get_ext(out_format));
				}
			if (out_format == 2 || out_format == 3) {
				fbc->set(Pass3_Name, "compression", exr_comp);	
			}		
		fbc->set(Pass3_Name, "filename", render_dir_cur_pass);
		fbc->set(Pass3_Name, "filetype", get_ext(out_format));
		fbc->set(Pass3_Name, "datatype", get_type(out_format));
		fbc->set(Pass3_Name, "user", true);
		fbc->set(Pass3_Name, "useopacity", true);
		if(filtering == miTRUE)
			fbc->set(Pass3_Name, "filtering", true);
		else
			fbc->set(Pass3_Name, "filtering", false);
	}
	
	if(*mi_eval_boolean(&paras->UserPass4_pass))									//UserPass4 pass
	{
		miTag UserPass4_Name = *mi_eval_tag(&paras->UserPass4_str);
		if (!UserPass4_Name)
			return miFALSE;
		char *Pass4_Name = (char*)mi_db_access(UserPass4_Name);
		mi_db_unpin(UserPass4_Name);
		
		char render_dir_cur_pass[MAX_PATH_L];
		strcpy(render_dir_cur_pass, render_dir);

			if(out_format != 2) {
				get_render_file_func(state, render_dir_cur_pass, name, Pass4_Name, frame_padding, get_ext(out_format));
			} else {
				get_render_file_func(state, render_dir_cur_pass, name, "single", frame_padding, get_ext(out_format));
				}
			if (out_format == 2 || out_format == 3) {
				fbc->set(Pass4_Name, "compression", exr_comp);	
			}		
		fbc->set(Pass4_Name, "filename", render_dir_cur_pass);
		fbc->set(Pass4_Name, "filetype", get_ext(out_format));
		fbc->set(Pass4_Name, "datatype", get_type(out_format));
		fbc->set(Pass4_Name, "user", true);
		fbc->set(Pass4_Name, "useopacity", true);
		if(filtering == miTRUE)
			fbc->set(Pass4_Name, "filtering", true);
		else
			fbc->set(Pass4_Name, "filtering", false);
	}
	
	//stereo camera
	if( *mi_eval_boolean(&paras->modifyCamera) )
	{
		mi_info("SettingsShader: Modify camera");
		char *camName = (char *)mi_api_tag_lookup(state->camera_inst);
		miInteger mode = *mi_eval_integer(&paras->mode);
		if( mode > 0)
		{
			mi_info("SettingsShader: Settings stereo info for camera: %s", camName);
			miScalar eyeDist = *mi_eval_scalar(&paras->eyeDist);

			const miCamera *cam = state->camera;
			miInteger *st = (miInteger *)&cam->stereo;
			*st = mode;
			float *sep = (float *)&cam->eye_separation;
			*sep = eyeDist;
		}
	}
}
	return(miTRUE);
}

DLLEXPORT int saveEyesOutput_version()
{
	return (1);
}

DLLEXPORT miBoolean saveEyesOutput(
	miColor                *result,
	miState                 *state,
	void   *paras)
{
	return miTRUE;
}

}
