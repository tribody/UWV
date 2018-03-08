/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 2007 Adobe Systems Incorporated                       */
/* All Rights Reserved.                                            */
/*                                                                 */
/* NOTICE:  All information contained herein is, and remains the   */
/* property of Adobe Systems Incorporated and its suppliers, if    */
/* any.  The intellectual and technical concepts contained         */
/* herein are proprietary to Adobe Systems Incorporated and its    */
/* suppliers and may be covered by U.S. and Foreign Patents,       */
/* patents in process, and are protected by trade secret or        */
/* copyright law.  Dissemination of this information or            */
/* reproduction of this material is strictly forbidden unless      */
/* prior written permission is obtained from Adobe Systems         */
/* Incorporated.                                                   */
/*                                                                 */
/*******************************************************************/

/*	UWV.cpp	

	This is a compiling husk of a project. Fill it in with interesting
	pixel processing code.
	
	Revision history: 

	1.0 (seemed like a good idea at the time)	bbb		6/1/2002

	1.0 Okay, I'm leaving the version at 1.0,	bbb		2/15/2006
		for obvious reasons; you're going to 
		copy these files directly! This is the
		first XCode version, though.

	1.0	Let's simplify this barebones sample	zal		11/11/2010

*/

#include "UWV.h"



A_long mesh_width, mesh_height_1, src_width=0, src_height=0; // 图片和网格大小信息
PF_EffectWorld *srcL = NULL, *srcM = NULL, *srcR = NULL;

// I -- fundalmental matrix
// Homo -- transform matrix
PF_FloatMatrix *I_L2M = NULL, *I_R2M = NULL, *Homo_L2M = NULL, *Homo_R2M = NULL;

// Processed Image Formats
A_long width_output;
A_long heigh_output;

// add by chromiumikx
PF_ParamDef checkout;
std::vector<PF_EffectWorld *>src_imgs( 12, NULL );

static float focal_length = UWV_ESTIMATION_DFLT;

static bool correct_order_flag = FALSE,
            correcting_order_flag = FALSE,
            flag_calc_homog = false,
            cal_success = false,
			mosaic_flag = false;

static int proj_method = UWV_METHOD_DFLT;

// add by chromiumikx
static int num_selected_imgs;
static int first_click_img;
static int second_click_img;
static std::vector<PF_EffectWorld **> imgs_prt{&srcL, &srcM, &srcR};
static PF_EffectWorld *dest_back_layer; // background image
static bool ordering_done = false;
static std::vector<float>lapped_ratios(12, UWV_RATIO_DFLT);
static std::vector<int>frame_shifts(12, UWV_FRAME_SHIFT_DFLT);

static std::vector<bool>flag_which_imported(12, false); // use for mark which view have been imported
static int flag_now_imported; // use for mark which view is being imported 
static Mat32f mat_result_img;



static PF_Err 
About (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	
	suites.ANSICallbacksSuite1()->sprintf(	out_data->return_msg,
											"%s v%d.%d\r%s",
											STR(StrID_Name), 
											MAJOR_VERSION, 
											MINOR_VERSION, 
											STR(StrID_Description));
	return PF_Err_NONE;
}

static PF_Err 
GlobalSetup (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	out_data->my_version = PF_VERSION(	MAJOR_VERSION, 
										MINOR_VERSION,
										BUG_VERSION, 
										STAGE_VERSION, 
										BUILD_VERSION);

	// Here bind PiPL.r tightly ...
	out_data->out_flags =  PF_OutFlag_DEEP_COLOR_AWARE // just 16bpc, not 32bpc
						   |	PF_OutFlag_PIX_INDEPENDENT //pixel independent
						   |	PF_OutFlag_I_EXPAND_BUFFER // expand the output buffer
						   |	PF_OutFlag_SEND_UPDATE_PARAMS_UI // set to receive PF_Cmd_UPDATE_PARAMS_UI
		                   |    PF_OutFlag_CUSTOM_UI; // set to receive PF_Cmd_Event，不使用
	
	// honor the collapse ability of each parameters group
	out_data->out_flags2 = PF_OutFlag2_PARAM_GROUP_START_COLLAPSED_FLAG;

	I_L2M = new PF_FloatMatrix;
	I_L2M->mat[0][0] = I_L2M->mat[1][1] = I_L2M->mat[2][2] = 1;

	I_R2M = new PF_FloatMatrix;
	I_R2M->mat[0][0] = I_R2M->mat[1][1] = I_R2M->mat[2][2] = 1;

	Homo_L2M = new PF_FloatMatrix;
	Homo_L2M->mat[0][0] = Homo_L2M->mat[1][1] = Homo_L2M->mat[2][2] = 1;

	Homo_R2M = new PF_FloatMatrix;
	Homo_R2M->mat[0][0] = Homo_R2M->mat[1][1] = Homo_R2M->mat[2][2] = 1;
	return PF_Err_NONE;
}

static PF_Err
GlobalSetdown(
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output)
{
	delete I_L2M;
	delete I_R2M;
	delete Homo_L2M;
	delete Homo_R2M;

	return PF_Err_NONE;
}

static PF_Err
SequenceSetup(
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output)
{
	mySequenceData* mySD_P;

	if (out_data->sequence_data) {
		PF_DISPOSE_HANDLE(out_data->sequence_data);
	}
	out_data->sequence_data = PF_NEW_HANDLE(sizeof(mySequenceData));
	if (!out_data->sequence_data) {
		return PF_Err_INTERNAL_STRUCT_DAMAGED;
	}

	mySD_P = *(mySequenceData**)out_data->sequence_data;

	mySD_P->Homo_L= new PF_FloatMatrix;
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 8; j++)
			mySD_P->Homo_L->mat[i][j] = 0;

	mySD_P->Homo_L->mat[0][0] = mySD_P->Homo_L->mat[1][1] = mySD_P->Homo_L->mat[2][2] = 1;

	mySD_P->Homo_R = new PF_FloatMatrix;
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 8; j++)
			mySD_P->Homo_R->mat[i][j] = 0;
	mySD_P->Homo_R->mat[0][0] = mySD_P->Homo_R->mat[1][1] = mySD_P->Homo_R->mat[2][2] = 1;

	return PF_Err_NONE;
}

static PF_Err
SequenceSetdown(
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output)
{
	if (in_data->sequence_data) {
		mySequenceData *mySD_P = *(mySequenceData**)out_data->sequence_data;
		delete mySD_P->Homo_L;
		delete mySD_P->Homo_R;
		PF_DISPOSE_HANDLE(in_data->sequence_data);
		out_data->sequence_data = NULL;
	}
	return PF_Err_NONE;
}

static PF_Err
SequenceFlatten(
	PF_InData		*in_data,
	PF_OutData		*out_data)
{
	PF_Err err = PF_Err_NONE;
	AEGP_SuiteHandler suites(in_data->pica_basicP);

	// Make a flat copy of whatever is in the unflat seq data handed to us. 
	if (in_data->sequence_data) {
		mySequenceData* mySD_P = reinterpret_cast<mySequenceData*>(DH(in_data->sequence_data));

		if (mySD_P) {
			PF_Handle flat_seq_dataH = suites.HandleSuite1()->host_new_handle(sizeof(flatSequenceData));

			if (flat_seq_dataH) {
				flatSequenceData*	flatSD_P = reinterpret_cast<flatSequenceData*>(suites.HandleSuite1()->host_lock_handle(flat_seq_dataH));

				if (flatSD_P) {
					AEFX_CLR_STRUCT(*flatSD_P);
					flatSD_P->L2M[0] = mySD_P->Homo_L->mat[0][0];
					flatSD_P->L2M[1] = mySD_P->Homo_L->mat[0][1];
					flatSD_P->L2M[2] = mySD_P->Homo_L->mat[0][2];
					flatSD_P->L2M[3] = mySD_P->Homo_L->mat[1][0];
					flatSD_P->L2M[4] = mySD_P->Homo_L->mat[1][1];
					flatSD_P->L2M[5] = mySD_P->Homo_L->mat[1][2];
					flatSD_P->L2M[6] = mySD_P->Homo_L->mat[2][0];
					flatSD_P->L2M[7] = mySD_P->Homo_L->mat[2][1];
					flatSD_P->L2M[8] = mySD_P->Homo_L->mat[2][2];

					flatSD_P->R2M[0] = mySD_P->Homo_R->mat[0][0];
					flatSD_P->R2M[0] = mySD_P->Homo_R->mat[0][0];
					flatSD_P->R2M[0] = mySD_P->Homo_R->mat[0][0];
					flatSD_P->R2M[0] = mySD_P->Homo_R->mat[0][0];
					flatSD_P->R2M[0] = mySD_P->Homo_R->mat[0][0];
					flatSD_P->R2M[0] = mySD_P->Homo_R->mat[0][0];
					flatSD_P->R2M[0] = mySD_P->Homo_R->mat[0][0];
					flatSD_P->R2M[0] = mySD_P->Homo_R->mat[0][0];
					flatSD_P->R2M[0] = mySD_P->Homo_R->mat[0][0];

					suites.HandleSuite1()->host_dispose_handle(in_data->sequence_data);
					out_data->sequence_data = flat_seq_dataH;
					suites.HandleSuite1()->host_unlock_handle(flat_seq_dataH);
				}
			}
			else {
				err = PF_Err_INTERNAL_STRUCT_DAMAGED;
			}
		}
	}
	else {
		err = PF_Err_INTERNAL_STRUCT_DAMAGED;
	}
	return err;
}

static PF_Err
SequenceResetup(
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output)
{
	PF_Err err = PF_Err_NONE;
	AEGP_SuiteHandler suites(in_data->pica_basicP);

	// We got here because we're either opening a project w/saved (flat) sequence data,
	// or we've just been asked to flatten our sequence data (for a save) and now 
	// we're blowing it back up.

	if (in_data->sequence_data) {
		// get the flatten sequence data from the disk?
		flatSequenceData *flatSD_P = reinterpret_cast<flatSequenceData*>(DH(in_data->sequence_data));

		if (flatSD_P) {
			PF_Handle unflat_seq_dataH = suites.HandleSuite1()->host_new_handle(sizeof(mySequenceData));

			if (unflat_seq_dataH) {
				// produce an unflatten sequence pointer
				mySequenceData* mySD_P = reinterpret_cast<mySequenceData*>(suites.HandleSuite1()->host_lock_handle(unflat_seq_dataH));

				if (mySD_P) {
					AEFX_CLR_STRUCT(*mySD_P);
					mySD_P->Homo_L = new PF_FloatMatrix;

					mySD_P->Homo_R = new PF_FloatMatrix;

					mySD_P->Homo_L->mat[0][0] = flatSD_P->L2M[0];
					mySD_P->Homo_L->mat[0][1] = flatSD_P->L2M[1];
					mySD_P->Homo_L->mat[0][2] = flatSD_P->L2M[2];
					mySD_P->Homo_L->mat[1][0] = flatSD_P->L2M[3];
					mySD_P->Homo_L->mat[1][1] = flatSD_P->L2M[4];
					mySD_P->Homo_L->mat[1][2] = flatSD_P->L2M[5];
					mySD_P->Homo_L->mat[2][0] = flatSD_P->L2M[6];
					mySD_P->Homo_L->mat[2][1] = flatSD_P->L2M[7];
					mySD_P->Homo_L->mat[2][2] = flatSD_P->L2M[8];


					mySD_P->Homo_R->mat[0][0] = flatSD_P->R2M[0];
					mySD_P->Homo_R->mat[0][1] = flatSD_P->R2M[1];
					mySD_P->Homo_R->mat[0][2] = flatSD_P->R2M[2];
					mySD_P->Homo_R->mat[1][0] = flatSD_P->R2M[3];
					mySD_P->Homo_R->mat[1][1] = flatSD_P->R2M[4];
					mySD_P->Homo_R->mat[1][2] = flatSD_P->R2M[5];
					mySD_P->Homo_R->mat[2][0] = flatSD_P->R2M[6];
					mySD_P->Homo_R->mat[2][1] = flatSD_P->R2M[7];
					mySD_P->Homo_R->mat[2][2] = flatSD_P->R2M[8];

					out_data->sequence_data = unflat_seq_dataH;
					suites.HandleSuite1()->host_unlock_handle(unflat_seq_dataH);
				}
			}
			else {
				err = PF_Err_INTERNAL_STRUCT_DAMAGED;
			}
		}
	}
	return err;
}


static PF_Err 
ParamsSetup (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err		err		= PF_Err_NONE;
	PF_ParamDef	def;	

	/* Import */
	AEFX_CLR_STRUCT(def);
	def.flags = PF_ParamFlag_START_COLLAPSED; // Controls the twirl-state of a topic spinner
	PF_ADD_TOPIC(STR(StrID_Import), IMPORT_BEG_ID);
	// ID和StrID位置相差2: ID + 2 = StrID
	int i;
	for (i = 0; i < 12; i++) {
		AEFX_CLR_STRUCT(def);
		def.flags = PF_ParamFlag_SUPERVISE	// set to receive PF_Cmd_USER_CHANGED_PARAM
			| PF_ParamFlag_CANNOT_TIME_VARY; // do not vary with time, otherwise will be changed after first applied
		PF_ADD_LAYER(STR(i+UWV_IMPORT_VIEW_1_ID+2), -1, i+ UWV_IMPORT_VIEW_1_ID);
	}
	AEFX_CLR_STRUCT(def);
	PF_END_TOPIC(IMPORT_END_ID);

	/* Lapped percentage */
	AEFX_CLR_STRUCT(def);
	def.flags |= PF_ParamFlag_START_COLLAPSED;
	PF_ADD_TOPIC(STR(StrID_Percentage), PERCENTAGE_BEG_ID);
	for (i = 0; i < 12; i++) {
		AEFX_CLR_STRUCT(def);
		def.flags = PF_ParamFlag_SUPERVISE
			| PF_ParamFlag_CANNOT_TIME_VARY
			| PF_ParamFlag_START_COLLAPSED;
		PF_ADD_SLIDER(STR(i + UWV_PERCENTAGE_RATIO_1_ID + 2),
			UWV_RATIO_MIN,
			UWV_RATIO_MAX,
			UWV_RATIO_MIN,
			UWV_RATIO_MAX,
			UWV_RATIO_DFLT,
			i + UWV_PERCENTAGE_RATIO_1_ID);
	}
	AEFX_CLR_STRUCT(def);
	PF_END_TOPIC(PERCENTAGE_END_ID);

	/* Frame shift */
	AEFX_CLR_STRUCT(def);
	def.flags |= PF_ParamFlag_START_COLLAPSED;
	PF_ADD_TOPIC(STR(StrID_Frame_Shift), FRAME_SHIFT_BEG_ID);
	for (i = 0; i < 12; i++) {
		AEFX_CLR_STRUCT(def);
		def.flags = PF_ParamFlag_SUPERVISE
			| PF_ParamFlag_CANNOT_TIME_VARY;
		PF_ADD_SLIDER(STR(i + UWV_FRAME_SHIFT_1_ID + 2),
			UWV_FRAME_SHIFT_MIN,
			UWV_FRAME_SHIFT_MAX,
			UWV_FRAME_SHIFT_MIN,
			UWV_FRAME_SHIFT_MAX,
			UWV_FRAME_SHIFT_DFLT, // DFLT: 默认值
			i + UWV_FRAME_SHIFT_1_ID);
	}
	AEFX_CLR_STRUCT(def);
	PF_END_TOPIC(FRAME_SHIFT_END_ID);

	/* SETTING stitching */
	AEFX_CLR_STRUCT(def);
	def.flags |= PF_ParamFlag_START_COLLAPSED;
	PF_ADD_TOPIC(STR(StrID_UWV), STITCH_BEG_ID);

	AEFX_CLR_STRUCT(def);
	def.flags |= PF_ParamFlag_SUPERVISE;
	PF_ADD_POPUP(STR(StrID_Projection),
		UWV_METHOD_NUM,
		UWV_METHOD_DFLT,
		STR(StrID_Projection_Type),
		METHOD_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDER(STR(StrID_Focal_Lenghth),
		UWV_ESTIMATION_MIN,
		UWV_ESTIMATION_MAX,
		UWV_ESTIMATION_MIN,
		UWV_ESTIMATION_MAX,
		AEFX_AUDIO_DEFAULT_CURVE_TOLERANCE,
		UWV_ESTIMATION_DFLT,
		UWV_ESTIMATION_PREC,
		0, 1,
		FOCAL_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_BUTTON(STR(StrID_Homography), "Calculate", 0,
		PF_ParamFlag_SUPERVISE, HOMOGRAPHY_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_BUTTON(STR(StrID_Mosaic), "Stitch",
		PF_PUI_DISABLED,
		PF_ParamFlag_SUPERVISE,
		MOSAIC_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_CHECKBOX(STR(StrID_Preview),
		STR(StrID_Preview),
		TRUE,
		0,
		PREVIEW_ID);

	AEFX_CLR_STRUCT(def);
	PF_END_TOPIC(STITCH_END_ID);

	/* 添加UI事件捕捉层，不使用 */
	if (!err)
	{
		PF_CustomUIInfo			ci;

		AEFX_CLR_STRUCT(ci);

		ci.events = PF_CustomEFlag_LAYER |
			PF_CustomEFlag_COMP;
		ci.comp_ui_width =
			ci.comp_ui_height = 0;

		ci.comp_ui_alignment = PF_UIAlignment_NONE;

		ci.layer_ui_width =
			ci.layer_ui_height = 0;

		ci.layer_ui_alignment = PF_UIAlignment_NONE;

		ci.preview_ui_width =
			ci.preview_ui_height = 0;

		ci.layer_ui_alignment = PF_UIAlignment_NONE;

		err = (*(in_data->inter.register_ui))(in_data->effect_ref, &ci);
	}


	out_data->num_params = UWV_NUM_PARAMS;

	return err;
}

static
PF_Err
FrameSetup(
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output)
{
	PF_Err				err = PF_Err_NONE;

	PF_FpLong border = params[UWV_INPUT]->u.fs_d.value;
	return err;
}

static PF_Err
MakeParamCopy(
	PF_ParamDef *actual[],	/* >> */
	PF_ParamDef copy[])		/* << */
{
	for (A_short iS = 0; iS < UWV_NUM_PARAMS; ++iS) {
		AEFX_CLR_STRUCT(copy[iS]);	// clean params are important!
	}
	copy[UWV_INPUT]			= *actual[UWV_INPUT];
	copy[UWV_IMPORT_BEG]	= *actual[IMPORT_BEG_ID];
	int i;
	for (i = 0; i < 12; i++) {
		copy[i+ UWV_IMPORT_VIEW_1] = *actual[i+ UWV_IMPORT_VIEW_1];
	}
	// copy[UWV_IMPORT_CORRECTORDER]  = *actual[UWV_IMPORT_CORRECTORDER];

	copy[PERCENTAGE_BEG_ID] = *actual[PERCENTAGE_BEG_ID];
	for (i = 0; i < 12; i++) {
		copy[i + UWV_PERCENTAGE_RATIO_1] = *actual[i + UWV_PERCENTAGE_RATIO_1];
	}

	copy[FRAME_SHIFT_BEG_ID] = *actual[FRAME_SHIFT_BEG_ID];
	for (i = 0; i < 12; i++) {
		copy[i + UWV_FRAME_SHIFT_1] = *actual[i + UWV_FRAME_SHIFT_1];
	}

	copy[STITCH_BEG_ID] = *actual[STITCH_BEG_ID];
	copy[UWV_PROJECTION_METHOD] = *actual[UWV_PROJECTION_METHOD];
	copy[UWV_FOCAL] = *actual[UWV_FOCAL];
	copy[UWV_HOMOGRAPHY] = *actual[UWV_HOMOGRAPHY];
	copy[UWV_MOSAIC] = *actual[UWV_MOSAIC];
	return PF_Err_NONE;
}

static PF_Err
UserChangedParam(
	PF_InData						*in_data,
	PF_OutData						*out_data,
	PF_ParamDef						*params[],
	PF_LayerDef						*outputP,
	const PF_UserChangedParamExtra	*which_hitP)
{
	PF_Err err = PF_Err_NONE;

	switch (which_hitP->param_index) {

	case UWV_IMPORT_VIEW_1 + 0:
	case UWV_IMPORT_VIEW_1 + 1:
	case UWV_IMPORT_VIEW_1 + 2:
	case UWV_IMPORT_VIEW_1 + 3:
	case UWV_IMPORT_VIEW_1 + 4:
	case UWV_IMPORT_VIEW_1 + 5:
	case UWV_IMPORT_VIEW_1 + 6:
	case UWV_IMPORT_VIEW_1 + 7:
	case UWV_IMPORT_VIEW_1 + 8:
	case UWV_IMPORT_VIEW_1 + 9:
	case UWV_IMPORT_VIEW_1 + 10:
	case UWV_IMPORT_VIEW_1 + 11:
		flag_now_imported = (which_hitP->param_index) - UWV_IMPORT_VIEW_1;
		flag_which_imported[(which_hitP->param_index) - UWV_IMPORT_VIEW_1] = true; // flag_which_imported[0] tags UWV_IMPORT_VIEW_1 if be selected
		out_data->out_flags |= PF_OutFlag_FORCE_RERENDER; // 每次导入图片都重新渲染
		break;

	case UWV_PERCENTAGE_RATIO_1 + 0:
	case UWV_PERCENTAGE_RATIO_1 + 1:
	case UWV_PERCENTAGE_RATIO_1 + 2:
	case UWV_PERCENTAGE_RATIO_1 + 3:
	case UWV_PERCENTAGE_RATIO_1 + 4:
	case UWV_PERCENTAGE_RATIO_1 + 5:
	case UWV_PERCENTAGE_RATIO_1 + 6:
	case UWV_PERCENTAGE_RATIO_1 + 7:
	case UWV_PERCENTAGE_RATIO_1 + 8:
	case UWV_PERCENTAGE_RATIO_1 + 9:
	case UWV_PERCENTAGE_RATIO_1 + 10:
	case UWV_PERCENTAGE_RATIO_1 + 11:
		lapped_ratios[which_hitP->param_index] = params[which_hitP->param_index]->u.sd.value / 100.f;
		break;

	case UWV_FRAME_SHIFT_1 + 0:
	case UWV_FRAME_SHIFT_1 + 1:
	case UWV_FRAME_SHIFT_1 + 2:
	case UWV_FRAME_SHIFT_1 + 3:
	case UWV_FRAME_SHIFT_1 + 4:
	case UWV_FRAME_SHIFT_1 + 5:
	case UWV_FRAME_SHIFT_1 + 6:
	case UWV_FRAME_SHIFT_1 + 7:
	case UWV_FRAME_SHIFT_1 + 8:
	case UWV_FRAME_SHIFT_1 + 9:
	case UWV_FRAME_SHIFT_1 + 10:
	case UWV_FRAME_SHIFT_1 + 11:
		frame_shifts[which_hitP->param_index] = params[which_hitP->param_index]->u.sd.value;
		break;

	case UWV_PROJECTION_METHOD:
		proj_method = params[UWV_PROJECTION_METHOD]->u.pd.value;
		break;

	case UWV_FOCAL:
		focal_length = (A_FpShort)params[UWV_FOCAL]->u.fs_d.value;
		break;
	
	case UWV_HOMOGRAPHY:
		flag_calc_homog = true;
		out_data->out_flags |= PF_OutFlag_FORCE_RERENDER;
		break;

	case UWV_MOSAIC:
		mosaic_flag = true;
		out_data->out_flags |= PF_OutFlag_FORCE_RERENDER;
		break;
	}
	return err;
}

static PF_Err
UpdateParameterUI(
	PF_InData			*in_data,
	PF_OutData			*out_data,
	PF_ParamDef			*params[],
	PF_LayerDef			*outputP)
{
	PF_Err				err = PF_Err_NONE;

	AEGP_SuiteHandler suites(in_data->pica_basicP);
	PF_ParamDef param_copy[UWV_NUM_PARAMS];
	ERR(MakeParamCopy(params, param_copy));

	// expand the collapsed items
	if ((param_copy[IMPORT_BEG_ID].flags & PF_ParamFlag_COLLAPSE_TWIRLY)
		&& (param_copy[PERCENTAGE_BEG_ID].flags & PF_ParamFlag_COLLAPSE_TWIRLY)
		&& (param_copy[FRAME_SHIFT_BEG_ID].flags & PF_ParamFlag_COLLAPSE_TWIRLY)
		&& (param_copy[STITCH_BEG_ID].flags & PF_ParamFlag_COLLAPSE_TWIRLY)) {
		param_copy[IMPORT_BEG_ID].flags			&= ~PF_ParamFlag_COLLAPSE_TWIRLY;
		param_copy[PERCENTAGE_BEG_ID].flags		&= ~PF_ParamFlag_COLLAPSE_TWIRLY;
		param_copy[FRAME_SHIFT_BEG_ID].flags	&= ~PF_ParamFlag_COLLAPSE_TWIRLY;
		param_copy[STITCH_BEG_ID].flags			&= ~PF_ParamFlag_COLLAPSE_TWIRLY;

		// auto expand RATIO_1 and FRAME_SHIFT_1
		param_copy[UWV_PERCENTAGE_RATIO_1].flags &= ~PF_ParamFlag_COLLAPSE_TWIRLY;
		param_copy[UWV_FRAME_SHIFT_1].flags &= ~PF_ParamFlag_COLLAPSE_TWIRLY;

		ERR(suites.ParamUtilsSuite3()->PF_UpdateParamUI(in_data->effect_ref,
			IMPORT_BEG_ID,
			&param_copy[IMPORT_BEG_ID]));
		ERR(suites.ParamUtilsSuite3()->PF_UpdateParamUI(in_data->effect_ref,
			PERCENTAGE_BEG_ID,
			&param_copy[PERCENTAGE_BEG_ID]));
		ERR(suites.ParamUtilsSuite3()->PF_UpdateParamUI(in_data->effect_ref,
			FRAME_SHIFT_BEG_ID,
			&param_copy[FRAME_SHIFT_BEG_ID]));
		ERR(suites.ParamUtilsSuite3()->PF_UpdateParamUI(in_data->effect_ref,
			STITCH_BEG_ID,
			&param_copy[STITCH_BEG_ID]));
		
		// auto expand RATIO_1 and FRAME_SHIFT_1
		ERR(suites.ParamUtilsSuite3()->PF_UpdateParamUI(in_data->effect_ref,
			UWV_PERCENTAGE_RATIO_1_ID,
			&param_copy[UWV_PERCENTAGE_RATIO_1_ID]));
		ERR(suites.ParamUtilsSuite3()->PF_UpdateParamUI(in_data->effect_ref,
			UWV_FRAME_SHIFT_1_ID,
			&param_copy[UWV_FRAME_SHIFT_1_ID]));
	}

	if (!err && params[UWV_PROJECTION_METHOD]->u.pd.value == 2) {
		param_copy[UWV_FOCAL].ui_flags &= ~PF_PUI_DISABLED;
		ERR(suites.ParamUtilsSuite3()->PF_UpdateParamUI(in_data->effect_ref,
			UWV_FOCAL,
			&param_copy[UWV_FOCAL]));
	}
	else {
		param_copy[UWV_FOCAL].ui_flags |= PF_PUI_DISABLED;
		ERR(suites.ParamUtilsSuite3()->PF_UpdateParamUI(in_data->effect_ref,
			UWV_FOCAL,
			&param_copy[UWV_FOCAL]));
	}
	
return		err;
}

static PF_Err
EwToMat(PF_InData *in_data, PF_EffectWorld *imgE, Mat32f& imgMat) {
	int w = imgE->width,
		h = imgE->height,
		rb = imgE->rowbytes; // 每次步进，下一像素

	PF_Pixel8 *pixelP = NULL;
	PF_GET_PIXEL_DATA8(imgE, NULL, &pixelP);

	for (int y = 0; y < imgMat.rows(); y++) {
		for (int x = 0; x < imgMat.cols(); x++) {
			imgMat.at(y, x, 0) = (float)(pixelP[x].blue)/255.0;
			imgMat.at(y, x, 1) = (float)(pixelP[x].green)/255.0;
			imgMat.at(y, x, 2) = (float)(pixelP[x].red)/255.0;
			//imgMat.at(y, x, 3) = pixelP[x].alpha;
		}
		pixelP = (PF_Pixel8*)((char*)pixelP + rb); //Move to the next line
	}
	return PF_Err_NONE;
}

static PF_Err
MatToEw(PF_InData *in_data, Mat32f *imgMat, PF_EffectWorld *imgE) {
	int w = imgMat->cols(),
		h = imgMat->rows(),
		rb = imgE->rowbytes;

	PF_Pixel8 *pixelP = NULL;
	PF_GET_PIXEL_DATA8(imgE, NULL, &pixelP);

	for (int y = 0; y < h; y++) { // 注意对应mat32f的格式
		for (int x = 0; x < w; x++) {
			pixelP[x].blue = 255 * imgMat->at(y, x, 0);
			pixelP[x].green = 255 * imgMat->at(y, x, 1);
			pixelP[x].red = 255 * imgMat->at(y, x, 2);
			pixelP[x].alpha = 255;//imgMat.at(y, x, 3);
		}
		pixelP = (PF_Pixel8*)((char*)pixelP + rb);
	}
	return PF_Err_NONE;
}

static PF_Err 
Render (
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err err = PF_Err_NONE;

	// 渲染输出时，在此传入当前帧
	PF_EffectWorld	*pure_color_layer = &params[UWV_INPUT]->u.ld; // 流程：把别的素材的当前帧当作参数（处理后）写到output上，

	// 读取当前将要处理的帧
	// 鼠标调节已经不可用
	// 每次rerender会重新checkout，会覆盖指针调整
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	PF_WorldSuite2 *wsP = NULL;
	PF_PixelFormat format;
	ERR(suites.Pica()->AcquireSuite(kPFWorldSuite, kPFWorldSuiteVersion2, (const void**)&wsP));
	int i;
	int num_selected_imgs = 0;
	// loop every selecting action, little bit low efficient
	for(i=0; i<12; i++)
	{
		// create layers
		// get images
		PF_CHECKOUT_PARAM(in_data,
			(i+2),
			(in_data->current_time + frame_shifts[i] * in_data->time_step),//+ff_left*in_data->time_step, 
			in_data->time_step,
			in_data->time_scale,
			&checkout);
		if (((&checkout.u.ld)->width != 0) && ((&checkout.u.ld)->width != 4096)) { // 判断当前IMPORT的不是空图片也不是纯色图层（宽度为4096）
			ERR(wsP->PF_GetPixelFormat((&checkout.u.ld), &format));
			src_imgs[i] = new  PF_EffectWorld;
			ERR(wsP->PF_NewWorld(in_data->effect_ref, (&checkout.u.ld)->width, (&checkout.u.ld)->height, 1, format, (src_imgs[i])));
			PF_COPY(&checkout.u.ld,
				src_imgs[i],
				NULL,
				NULL);
			num_selected_imgs = num_selected_imgs  + 1;
			// 预览图各子图的大小
			src_width = src_imgs[i]->width;
			src_height = src_imgs[i]->height;
		}
		PF_CHECKIN_PARAM(in_data, &checkout);
	}

	// 拼合当前预览图
	if (num_selected_imgs  != 0) {
		out_data->width = (in_data->width) / 4; // ??
		out_data->height = (in_data->height) / 4; // ??
		mesh_width = (output->width) / num_selected_imgs; // 拼接的view的数目
		mesh_height_1 = ((output->height - (src_height*mesh_width) / (src_width))) >> 1;
		for (i = 0; i < num_selected_imgs; i++) {
			if (src_imgs[i] != NULL) {
				PF_Rect destArea = { i*mesh_width, mesh_height_1, (i + 1)*mesh_width, output->height - mesh_height_1 };
				PF_COPY(src_imgs[i], output, NULL, &destArea);
				ordering_done = false; // 将调序后的输出渲染完，重置
			}
		}
	}


	/*
	 * TODO
	 * 1、参数预设
	 * 2、参数更新
	 * 3、图像拼接
	 */


	if (proj_method == 1) {	// 1 means default project method PLANAR
		// initial the planar stitcher

	} else {	// project method CYLINDER
		// initial the cylinder stitcher
	}

	// do the h calculation
	if (flag_calc_homog) {

		// 计算单应前，在此作数据转换
		// Mat32f mat_one_img(src_imgs[0]->height, src_imgs[0]->width, 3); // !!! mat32f数据结构：（height, witdth, channels） // no need
	    vector<Mat32f> mat_imgs;
		// cal_success = CalHomoInKeyFrame(matL, matM, matR);
		// stick stitcher.project here
		/* cal the H once */
		for (i = 0; i < num_selected_imgs; i++) {
			mat_imgs.emplace_back(src_imgs[0]->height, src_imgs[0]->width, 3);
			// 图片格式转换
			ERR(EwToMat(in_data, (src_imgs[i]), (mat_imgs[i])));
		}
		CylinderStitcher h_calc_er(move(mat_imgs));
		mat_result_img = h_calc_er.only_build_homog();
		if(1)cal_success=true;
		mat_result_img = crop(mat_result_img);
		
		ERR(wsP->PF_GetPixelFormat((src_imgs[0]), &format));
		PF_EffectWorld* ew_result_img  = new  PF_EffectWorld;
		ERR(wsP->PF_NewWorld(in_data->effect_ref, mat_result_img.width(), mat_result_img.height(), 1, format, (ew_result_img)));

		ERR(MatToEw(in_data, &(mat_result_img), ew_result_img));
		PF_COPY(ew_result_img, output, NULL, NULL);


		if (cal_success) {
			cal_success = false;
			params[UWV_MOSAIC]->ui_flags &= ~PF_PUI_DISABLED; // 允许 拼接
			PF_STRCPY(out_data->return_msg, "H matrix calculation finished!");
			out_data->out_flags |= PF_OutFlag_DISPLAY_ERROR_MESSAGE;
		} else {
			PF_STRCPY(out_data->return_msg, "H matrix calculation failed!");
			out_data->out_flags |= PF_OutFlag_DISPLAY_ERROR_MESSAGE;
		}
		flag_calc_homog = false;
	}

	// do the stitch work
	if (mosaic_flag) {
		/*

		// vector<Mat32f> mat_imgs;
		// cal_success = CalHomoInKeyFrame(matL, matM, matR);
		// stick stitcher.project here
		// cal the H once
		for (i = 0; i < num_selected_imgs; i++) {
			mat_imgs.emplace_back(src_imgs[0]->height, src_imgs[0]->width, 3);
			// 图片格式转换
			ERR(EwToMat(in_data, (src_imgs[i]), (mat_imgs[i])));
		}

		output_stitcher(move(mat_imgs)); 
		auto ret_2 = output_stitcher.bundle.blend();
		mat_result_img = perspective_correction(ret_2);
		mat_result_img = crop(mat_result_img);

		ERR(wsP->PF_GetPixelFormat((src_imgs[0]), &format));
		//PF_EffectWorld* ew_result_img = new  PF_EffectWorld;
		ERR(wsP->PF_NewWorld(in_data->effect_ref, mat_result_img.width(), mat_result_img.height(), 1, format, (ew_result_img)));

		ERR(MatToEw(in_data, &(mat_result_img), ew_result_img));
		mat_result_img.m_data = NULL;
		PF_COPY(ew_result_img, output, NULL, NULL); // 测试 debug

		// PF_STRCPY(out_data->return_msg, "Stitched finished!");
		// out_data->out_flags |= PF_OutFlag_DISPLAY_ERROR_MESSAGE;

		*/
	}


	return err;
}



PF_Err DllExport
EntryPointFunc (
	PF_Cmd			cmd,
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	void			*extra)
{
	PF_Err		err = PF_Err_NONE;
	
	try {
		switch (cmd) {
			case PF_Cmd_ABOUT:

				err = About(in_data,
							out_data,
							params,
							output);
				break;
				
			case PF_Cmd_GLOBAL_SETUP:

				err = GlobalSetup(	in_data,
									out_data,
									params,
									output);
				break;

			case PF_Cmd_GLOBAL_SETDOWN:

				err = GlobalSetdown(in_data,
									out_data,
									params,
									output);
				break;

			case PF_Cmd_PARAMS_SETUP:

				err = ParamsSetup(	in_data,
									out_data,
									params,
									output);
				break;

			case PF_Cmd_USER_CHANGED_PARAM:

				err = UserChangedParam(in_data,
					out_data,
					params,
					output,
					reinterpret_cast<const PF_UserChangedParamExtra *>(extra));
				break;
				
			case PF_Cmd_RENDER: // 注：取帧必须要在这里做，否则渲染的时候没法一直是最新帧

				err = Render(	in_data,
								out_data,
								params,
								output);
				break;

			case PF_Cmd_UPDATE_PARAMS_UI:

				err = UpdateParameterUI(in_data,
										out_data,
										params,
										output);
				break;

		}
	}
	catch(PF_Err &thrown_err){
		err = thrown_err;
	}
	return err;
};
