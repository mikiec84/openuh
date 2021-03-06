/*
  Copyright 2014 University of Houston. All Rights Reserved.
*/

/*
  Copyright UT-Battelle, LLC.  All Rights Reserved. 2014
  Oak Ridge National Laboratory
*/

/*
 * Copyright (C) 2009 Advanced Micro Devices, Inc.  All Rights Reserved.
 */

/*
 * Copyright (C) 2007. QLogic Corporation. All Rights Reserved.
 */

/*
 * Copyright 2003, 2004, 2005, 2006 PathScale, Inc.  All Rights Reserved.
 */

/*

  Copyright (C) 2000, 2001 Silicon Graphics, Inc.  All Rights Reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it would be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

  UT-BATTELLE, LLC AND THE GOVERNMENT MAKE NO REPRESENTATIONS AND DISCLAIM ALL
  WARRANTIES, BOTH EXPRESSED AND IMPLIED.  THERE ARE NO EXPRESS OR IMPLIED
  WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, OR THAT
  THE USE OF THE SOFTWARE WILL NOT INFRINGE ANY PATENT, COPYRIGHT, TRADEMARK,
  OR OTHER PROPRIETARY RIGHTS, OR THAT THE SOFTWARE WILL ACCOMPLISH THE
  INTENDED RESULTS OR THAT THE SOFTWARE OR ITS USE WILL NOT RESULT IN INJURY
  OR DAMAGE.  THE USER ASSUMES RESPONSIBILITY FOR ALL LIABILITIES, PENALTIES,
  FINES, CLAIMS, CAUSES OF ACTION, AND COSTS AND EXPENSES, CAUSED BY,
  RESULTING FROM OR ARISING OUT OF, IN WHOLE OR IN PART THE USE, STORAGE OR
  DISPOSAL OF THE SOFTWARE.

  Further, this software is distributed without any warranty that it is
  free of the rightful claim of any third person regarding infringement 
  or the like.  Any license provided herein, whether implied or 
  otherwise, applies only to this software file.  Patent licenses, if 
  any, provided herein do not apply to combinations of this program with 
  other software, or any other product whatsoever.  

  You should have received a copy of the GNU General Public License along
  with this program; if not, write the Free Software Foundation, Inc., 59
  Temple Place - Suite 330, Boston MA 02111-1307, USA.

  Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pky,
  Mountain View, CA 94043, or:

  http://www.sgi.com

  For further information regarding this notice, see:

  http://oss.sgi.com/projects/GenInfo/NoticeExplan

*/



#include <stdint.h>

#include <stdlib.h>
#include <iomanip>
#include <stdio.h>

#if defined(BUILD_OS_DARWIN)
#include <darwin_elf.h>
#else /* defined(BUILD_OS_DARWIN) */
#include <elf.h>
#endif /* defined(BUILD_OS_DARWIN) */

#ifdef DRAGON
#include <fstream>
#include <string>
#include <vector>
#endif

#include "defs.h"
#include "errors.h"
#include "mempool.h"
#include "tlog.h"                       // Generate_Tlog

#include "cgb.h"                        // CG_BROWSER, CGB_Initialize
#include "cgb_ipa.h"                    // CGB_IPA_{Initialize|Terminate}
#include "ipaa.h"                       // mod/ref analysis
#include "ipa_cg.h"			// IPA_CALL_GRAPH
#include "ipa_cprop.h"			// constant propagation
#include "ipa_inline.h"			// for IPA_INLINE
#include "ipa_option.h"                 // trace options
#include "ipa_pad.h"                    // padding related code
#include "ipa_preopt.h"                 // IPA_Preopt_Finalize
#include "ipa_section_annot.h"          // SECTION_FILE_ANNOT
#include "ipa_section_prop.h"           // IPA_ARRAY_DF_FLOW
#include "ipa_nested_pu.h"              // Build_Nested_Pu_Relations
#include "ipo_tlog_utils.h"		// Ipa_tlog

#include "wn_tree_util.h"

#include "ipa_chg.h"                    // Class hierarchy graph
#include "ipa_devirtual.h"              // Devirtualization

#include "ipo_defs.h"

#ifndef KEY
#include "inline_script_parser.h"
#else
extern void (*Preprocess_struct_access_p)(void);
#define Preprocess_struct_access (*Preprocess_struct_access_p)
#endif /* KEY */

#ifdef DRAGON
#include "ipc_symtab_merge.h"
extern "C" void cleanup_all_files (void);
extern const char* output_filename;
extern BOOL Dragon_Flag;
BOOL Dragon_Flag = FALSE;
/*************************************
        Lei Huang 11/23/02
   The Dragon_CFG_Phase is used in f90_lower() in cfg_ipl phase. however,
   the f90_lower() is also used in ipa_linker phase, so, set the
   Dragon_CFG_Phase as FALSE when it appears in ipa_linker.
******************************/
BOOL Dragon_CFG_Phase=FALSE;
#endif

#include "ipa_reorder.h"

#include "ipa_nystrom_alias_analyzer.h"

#ifdef OPENSHMEM_ANALYZER
#include <unistd.h>
#include "opt_alias_interface.h"

#ifndef _LIGHTWEIGHT_INLINER
extern const char* output_filename;
#endif

extern "C" void cleanup_all_files (void);
extern "C" void remove_temp_dir (void);
#endif

#include "ipa_pcg.h"

FILE* STDOUT = stdout; 

#ifdef OPENSHMEM_ANALYZER
extern FILE* makefile;
void IPA_OpenSHMEM_Check(IPA_CALL_GRAPH* cg);
#endif

#ifdef DRAGON
char* Dragon_Symbol_Name(INT i,char** function_name, IPA_NODE *ipan)
{
  SUMMARY_SYMBOL* symbol_array = IPA_get_symbol_array(ipan);
  SUMMARY_SYMBOL* ss = &symbol_array[i];
  if (ST_IDX_level(ss->St_idx()) > 1) {
    ST_IDX func_st_idx = ss->Get_st_idx_func();
    PU_IDX pu_idx = ST_pu(ST_ptr(func_st_idx));
    NODE_INDEX node_index = AUX_PU_node(Aux_Pu_Table[pu_idx]);
    IPA_NODE* cnode = IPA_Call_Graph->Graph()->Node_User(node_index);
    IPA_NODE_CONTEXT context(cnode);
    if (function_name != NULL)
      *function_name = ST_name(func_st_idx);
    return ST_name(ss->St_idx());
  } else {
    IPA_NODE_CONTEXT context(ipan);
    if (function_name != NULL)
      *function_name = NULL;
    return ST_name(ss->St_idx());
  }
}

void Dragon_Dump_Regions(IPA_NODE *ipan, int fileid, ofstream
    &outputfile, WN *node)
{
    static unsigned int region_counter = 0;
    SUMMARY_FILE_HEADER* file_header =
        IP_FILE_HDR_file_header(ipan->File_Header());

    INT regions_size = file_header->Get_regions_array_size();
    INT lb_index, lb_count,ub_index,ub_count,step_index,step_count;
    REGION_ARRAYS* region_array = IPA_get_region_array(ipan);
    PROJECTED_REGION* proj_region_array = IPA_get_proj_region_array(ipan);
    PROJECTED_NODE* proj_node_array = IPA_get_projected_node_array(ipan);
    LOOPINFO* loopinfo_array = IPA_get_loopinfo_array(ipan);
    TERM* term_array = IPA_get_term_array(ipan);

    TERM * tm;
    char *func_name=NULL;
    char *name=NULL;

    char defuse[20]="",pj_defuse[20]="",term_info[100]="";

    WN_TREE_CONTAINER<PRE_ORDER> wcpre(node);
    WN_TREE_CONTAINER<PRE_ORDER>::iterator wipre;

    for (INT i = 0; i < regions_size; i++)
    {

        REGION_ARRAYS* ra = &region_array[i];
        name = Dragon_Symbol_Name(ra->Get_sym_id(), &func_name,ipan);

        PROJECTED_REGION* pr = &proj_region_array[ra->Get_idx()];

        int proj_reg=(int) pr->Get_id();
        int regarr=(int) ra->Get_sym_id();
        for (INT j=0; j<ra->Get_count(); j++)
        {
            outputfile<<fileid<<", ";

            if (func_name == NULL || func_name[0] == '\0')
                outputfile <<i<<j<<", @, "<<name<<", ";
            else
                outputfile <<i<<j<<", "<<func_name<<", "<<name<<", ";

            outputfile<<ipan->Input_File_Name()<<", ";
            if (ra->Is_use())
                outputfile<< "USE, ";
            else if (ra->Is_def())
                outputfile<< "DEF, ";
            else if (ra->Is_passed())
                outputfile<< "PASSED, ";
            else if (ra->Is_may_def())
                outputfile<< "MAY_DEF, ";
            else if (ra->Is_may_use())
                outputfile<< "MAY_USE, ";
            else if (ra->Is_formal())
                outputfile<< "FORMAL, ";
            outputfile<<ra->Get_count()<<", ";
            outputfile <<(int) pr->Get_num_dims()<<", ";

            PROJECTED_NODE* pn = &proj_node_array[proj_reg];

            if ((int) pr->Get_id()!= -1) {

                if (pn->Is_unprojected()) {
                    outputfile<<"unprojected, , , ";

                    int check=0;
                    for (wipre = wcpre.begin(); wipre != wcpre.end(); ++wipre) {
                        WN* temp;
                        temp = wipre.Wn();

                        if (WN_operator(temp) == OPR_ARRAY) {
                            //get the array name
                            WN* base_node = WN_array_base(temp);
                            char* base_name = ST_name(WN_st_idx(base_node));

                            if (name==base_name) {

                                if (check==0) {
                                    //get the #dimensions in the array
                                    int num_dim = WN_num_dim(temp);

                                    //get the #kids in the tree.
                                    int kid_count = WN_kid_count(temp);

                                    //get the line number
                                    // int line_number=WN_linenum_fixed(temp);
                                    // int lin= (int) WN_linenum(temp);

                                    // SRCPOS srcpos = WN_Get_Linenum(node);
                                    // USRCPOS linepos;
                                    // USRCPOS_srcpos(linepos) = srcpos;
                                    // int line = USRCPOS_linenum(linepos);
                                    // outputfile<<line<<", ";

                                    //get the elemnt size and data type
                                    int data_type_size = WN_element_size(temp);
                                    outputfile<<data_type_size<<", ";
                                    if(data_type_size==1)
                                        outputfile<<"char, ";
                                    else if(data_type_size==2)
                                        outputfile<<"shortint, ";
                                    else if(data_type_size==4)
                                        outputfile<<"int, ";
                                    else if(data_type_size==8)
                                        outputfile<<"double, ";
                                    else {
                                        outputfile<<"othertype, ";
                                    }

                                    //get each dimension size and the total size and capacity
                                    int total_size=1;
                                    int total_size_bytes;
                                    int dimen[num_dim];
                                    int ind;
                                    int hexad;
                                    char buffer [33];
                                    for(int i=0; i<num_dim; i++) {
                                        WN *dim = WN_array_dim(temp,i);
                                        WN *index=WN_array_index(temp,i);
                                        dimen[i]= WN_const_val(dim);
                                        outputfile<<dimen[i];
                                        if(num_dim > 1) {
                                            outputfile<<"|";
                                        }
                                        if(dimen[i]==0)
                                            dimen[i]=1;
                                        total_size*=dimen[i];


                                    }
                                    outputfile<<", ";
                                    outputfile<<total_size<<", ";
                                    total_size_bytes=total_size*data_type_size;
                                    outputfile<<total_size_bytes<<", ";

                                    WN *index=WN_array_index(temp,0);
                                    ind= WN_const_val(index);
                                    hexad=ind*data_type_size;
                                    stringstream oss;
                                    string mystr;
                                    oss <<hex<<hexad;
                                    mystr=oss.str();
                                    outputfile<<mystr<<", ";

                                    int acc_dens=ra->Get_count()*100/total_size_bytes;
                                    outputfile<<acc_dens;
                                    outputfile<<" ";
                                    check+=1;
                                }
                            }
                        }
                    }

                    if(check==0) {
                        outputfile<<", , , , , , , , ";
                    }

                } else {

                    if (pn->Is_messy_lb()) {
                        outputfile<<"messy, ";
                    } else {
                        // for (INT k=0; k<pn->Get_lb_term_count(); k++)
                        // {
                        TERM* tm = &term_array[pn->Get_lb_term_index()];
                        switch (tm->Get_type()) {

                            case LTKIND_NONE:
                                outputfile<<"none, ";
                                break;

                            case LTKIND_CONST:
                                // outputfile<<"constant ";
                                outputfile<<(int) tm->Get_coeff()<<", ";
                                break;

                            case LTKIND_LINDEX:
                                outputfile<<"lindex";
                                outputfile<<"loop_index("<<(int) tm->Get_desc()<<")*"<<(int) tm->Get_coeff()<<", ";
                                break;

                            case LTKIND_SUBSCR:
                                outputfile<<"subscr";
                                outputfile<<"dim("<<(int) tm->Get_desc()<<")*"<<(int) tm->Get_coeff()<<", ";
                                break;

                            case LTKIND_IV:
                                outputfile<<"IVAR";
                                outputfile<<"IVAR["<<(int) tm->Get_desc()<<"]*"<<(int) tm->Get_coeff()<<", ";
                                break;
                        }
                        // }
                        // outputfile <<(int) pn->Get_lb_term_index()<<", ";
                        // outputfile <<(int) pn->Get_lb_term_count()<<", ";
                    }

                    if (pn->Is_messy_ub()) {
                        outputfile<<"messy, ";
                    } else {
                        TERM* tm = &term_array[pn->Get_ub_term_index()];
                        switch (tm->Get_type()) {

                            case LTKIND_NONE:
                                outputfile<<"none, ";
                                break;

                            case LTKIND_CONST:
                                // outputfile<<"constant ";
                                outputfile<<(int) tm->Get_coeff()<<", ";
                                break;

                            case LTKIND_LINDEX:
                                outputfile<<"lindex";
                                outputfile<<"loop_index("<<(int) tm->Get_desc()<<")*"<<(int) tm->Get_coeff()<<", ";
                                break;

                            case LTKIND_SUBSCR:
                                outputfile<<"subscr";
                                outputfile<<"dim("<<(int) tm->Get_desc()<<")*"<<(int) tm->Get_coeff()<<", ";
                                break;

                            case LTKIND_IV:
                                outputfile<<"IVAR";
                                outputfile<<"IVAR["<<(int) tm->Get_desc()<<"]*"<<(int) tm->Get_coeff()<<", ";
                                break;
                        }

                        // outputfile <<(int) pn->Get_ub_term_index()<<", ";
                        // outputfile <<(int) pn->Get_ub_term_count()<<", ";
                    }

                    if (pn->Is_messy_step()) {
                        outputfile<<"smessy, ";
                    } else {
                        TERM* tm = &term_array[pn->Get_step_term_index()];
                        switch (tm->Get_type()) {

                            case LTKIND_NONE:
                                outputfile<<"none, ";
                                break;

                            case LTKIND_CONST:
                                // outputfile<<"constant ";
                                outputfile<<(int) tm->Get_coeff()<<", ";
                                break;

                            case LTKIND_LINDEX:
                                outputfile<<"lindex";
                                outputfile<<"loop_index("<<(int) tm->Get_desc()<<")*"<<(int) tm->Get_coeff()<<", ";
                                break;

                            case LTKIND_SUBSCR:
                                outputfile<<"subscr";
                                outputfile<<"dim("<<(int) tm->Get_desc()<<")*"<<(int) tm->Get_coeff()<<", ";
                                break;

                            case LTKIND_IV:
                                outputfile<<"IVAR";
                                outputfile<<"IVAR["<<(int) tm->Get_desc()<<"]*"<<(int) tm->Get_coeff()<<", ";
                                break;
                        }

                    }

                    int check=0;
                    for (wipre = wcpre.begin(); wipre != wcpre.end(); ++wipre) {
                        WN* temp;
                        temp = wipre.Wn();

                        if (WN_operator(temp) == OPR_ARRAY) {
                            //get the array name
                            WN* base_node = WN_array_base(temp);
                            char* base_name = ST_name(WN_st_idx(base_node));
                            if(name==base_name) {
                                if(check==0) {
                                    //get the #dimensions in the array
                                    int num_dim = WN_num_dim(temp);

                                    //get the #kids in the tree.
                                    int kid_count = WN_kid_count(temp);

                                    //get the line number
                                    // int line_number=WN_linenum_fixed(temp);
                                    // int lin= (int) WN_linenum(temp);

                                    // SRCPOS srcpos = WN_Get_Linenum(node);
                                    // USRCPOS linepos;
                                    // USRCPOS_srcpos(linepos) = srcpos;
                                    // int line = USRCPOS_linenum(linepos);
                                    // outputfile<<line<<", ";

                                    //get the elemnt size and data type
                                    int data_type_size = WN_element_size(temp);
                                    outputfile<<data_type_size<<", ";
                                    if(data_type_size==1)
                                        outputfile<<"char, ";
                                    else if(data_type_size==2)
                                        outputfile<<"shortint, ";
                                    else if(data_type_size==4)
                                        outputfile<<"int, ";
                                    else if(data_type_size==8)
                                        outputfile<<"double, ";
                                    else {
                                        outputfile<<"othertype, ";
                                    }

                                    //get each dimension size and the total size and capacity
                                    int total_size=1;
                                    int total_size_bytes;
                                    int dimen[num_dim];
                                    int ind;
                                    int hexad;
                                    for(int i=0; i<num_dim; i++) {
                                        WN *dim = WN_array_dim(temp,i);
                                        WN *index=WN_array_index(temp,i);
                                        dimen[i]= WN_const_val(dim);
                                        outputfile<<dimen[i];
                                        if(num_dim > 1) {
                                            outputfile<<"|";
                                        }
                                        if(dimen[i]==0)
                                            dimen[i]=1;
                                        total_size*=dimen[i];


                                    }
                                    outputfile<<", ";
                                    outputfile<<total_size<<", ";
                                    total_size_bytes=total_size*data_type_size;
                                    outputfile<<total_size_bytes<<", ";

                                    WN *index=WN_array_index(temp,0);
                                    ind= WN_const_val(index);
                                    hexad=ind*data_type_size;
                                    stringstream oss;
                                    string mystr;
                                    oss <<hex<<hexad;
                                    mystr=oss.str();
                                    outputfile<<mystr<<", ";
                                    int acc_dens=ra->Get_count()*100/total_size_bytes;
                                    outputfile<<acc_dens;
                                    outputfile<<" ";
                                    check+=1;
                                }
                            }
                        }
                    }

                    if(check==0) {
                        outputfile<<", , , , , , , , ";
                    }
                }

                outputfile<<endl;

            } else {

                outputfile<<"nodeismessy, , , ";
                int check=0;

                for (wipre = wcpre.begin(); wipre != wcpre.end(); ++wipre) {
                    WN* temp;
                    temp = wipre.Wn();

                    if (WN_operator(temp) == OPR_ARRAY) {
                        //get the array name
                        WN* base_node = WN_array_base(temp);
                        char* base_name = ST_name(WN_st_idx(base_node));

                        if(name==base_name) {

                            if(check==0) {
                                //get the #dimensions in the array
                                int num_dim = WN_num_dim(temp);

                                //get the #kids in the tree.
                                int kid_count = WN_kid_count(temp);

                                //get the line number
                                // int line_number=WN_linenum_fixed(temp);
                                // int lin= (int) WN_linenum(temp);

                                // SRCPOS srcpos = WN_Get_Linenum(node);
                                // USRCPOS linepos;
                                // USRCPOS_srcpos(linepos) = srcpos;
                                // int line = USRCPOS_linenum(linepos);
                                // outputfile<<line<<", ";

                                //get the elemnt size and data type
                                int data_type_size = WN_element_size(temp);
                                outputfile<<data_type_size<<", ";
                                if(data_type_size==1)
                                    outputfile<<"char, ";
                                else if(data_type_size==2)
                                    outputfile<<"shortint, ";
                                else if(data_type_size==4)
                                    outputfile<<"int, ";
                                else if(data_type_size==8)
                                    outputfile<<"double, ";
                                else {
                                    outputfile<<"othertype, ";
                                }

                                //get each dimension size and the total size and capacity
                                int total_size=1;
                                int total_size_bytes;
                                int dimen[num_dim];
                                int ind;
                                int hexad;
                                char buffer [33];
                                for(int i=0; i<num_dim; i++) {
                                    WN *dim = WN_array_dim(temp,i);
                                    WN *index=WN_array_index(temp,i);
                                    dimen[i]= WN_const_val(dim);
                                    outputfile<<dimen[i];

                                    if(num_dim > 1) {
                                        outputfile<<"|";
                                    }

                                    if(dimen[i]==0)
                                        dimen[i]=1;

                                    total_size*=dimen[i];
                                }

                                outputfile<<", ";
                                outputfile<<total_size<<", ";
                                total_size_bytes=total_size*data_type_size;
                                outputfile<<total_size_bytes<<", ";

                                WN *index=WN_array_index(temp,0);
                                ind= WN_const_val(index);
                                hexad=ind*data_type_size;
                                stringstream oss;
                                string mystr;
                                oss <<hex<<hexad;
                                mystr=oss.str();
                                outputfile<<mystr<<", ";

                                int acc_dens=ra->Get_count()*100/total_size_bytes;
                                outputfile<<acc_dens;
                                outputfile<<" ";
                                check+=1;
                            }
                        }
                    }
                }

                if(check==0) {
                    outputfile<<", , , , , , , , ";
                }
                outputfile<<endl;
            }

            proj_reg++;

        } // need to change

        region_counter++;
    }
}


void  Dragon_Dump_Array_Regions(char *outname)
{

  ofstream outputfile(outname,ios::out);
  vector<string> filenames;
  IPA_NODE_ITER cg_iter(IPA_Call_Graph, PREORDER);

  for (cg_iter.First(); !cg_iter.Is_Empty(); cg_iter.Next())
  {
    IPA_NODE* ipan = cg_iter.Current();
    if (ipan)
    {
      bool dump_regions = true;
      string tempfile = ipan->Input_File_Name();
      if (filenames.size()>0)
      {
        for(int i=0; i<filenames.size(); i++)
        {
          //outputfile << filenames[i]<<"\n";
          if(filenames[i]==tempfile)
            dump_regions=false;

        }
      }

      // printf("\n*****entering :%s\n",ipan->Name());
      if(dump_regions)
      {
        filenames.push_back(tempfile);
        IPA_NODE_CONTEXT context(ipan);
        WN* node = ipan->Whirl_Tree();
        Dragon_Dump_Regions(ipan,filenames.size(),outputfile,node);

      }
      // printf("\n*****exiting :%s\n",ipan->Name());

    }

  }
  outputfile.close();
  //printf("\n Exited Dragon_Dump_Array_Regions\n");
}

#endif

//-----------------------------------------------------------------------
// NAME: Print_Array_Sections
// FUNCTION: Dump the array sections to the file 'fp'.
//-----------------------------------------------------------------------

static void Print_Array_Sections(const char buffer[])
{
  CG_BROWSER cgb_print;
  CGB_Initialize(&cgb_print, IPA_Call_Graph);
  IPA_NODE_ITER cg_iter(cgb_print.Ipa_Cg(), PREORDER);
  if (Get_Trace(TP_IPA, IPA_TRACE_SECTION_CORRECTNESS)) {
    fprintf(stdout, "%s\n", buffer);
    fprintf(TFile, "%s\n", buffer);
  } 
  if (Get_Trace(TP_PTRACE1, TP_PTRACE1_IPALNO))
    Generate_Tlog("IPA", "Array_Section", (SRCPOS) 0, "", "", "", buffer);
  for (cg_iter.First(); !cg_iter.Is_Empty(); cg_iter.Next()) {
    IPA_NODE* ipan = cg_iter.Current();
    if (ipan == NULL)
      continue;
    NODE_INDEX v = cgb_print.Find_Vertex(ipan);
    if (v == INVALID_NODE_INDEX)
       continue;
    if (Get_Trace(TP_IPA, IPA_TRACE_SECTION_CORRECTNESS)) { 
      fprintf(stdout, "%s\n", ipan->Name());
      fprintf(TFile, "%s\n", ipan->Name());
    } 
    cgb_print.Set_Cnode(ipan);
    cgb_print.Set_Cvertex(v);
    IPA_NODE_SECTION_INFO* ipas = ipan->Section_Annot();
    SECTION_FILE_ANNOT* ipaf = IP_FILE_HDR_section_annot(ipan->File_Header());
    if (ipas == NULL || ipaf == NULL)
      continue;
    if (Get_Trace(TP_IPA, IPA_TRACE_SECTION_CORRECTNESS)) {
      if (cgb_print.Cnode()->Summary_Proc() != NULL
          && cgb_print.Cnode()->Summary_Proc()->Has_incomplete_array_info())
        fprintf(stdout, "INCOMPLETE ARRAY INFO\n");
      cgb_print.Mod_Ref_Formals(stdout);
      cgb_print.Mod_Ref_Commons(stdout);
      cgb_print.Mod_Ref_Formals(TFile);
      cgb_print.Mod_Ref_Commons(TFile);
    }
    if (Get_Trace(TP_PTRACE1, TP_PTRACE1_IPALNO)) {
      cgb_print.Tlog_Mod_Ref_Formals();
      cgb_print.Tlog_Mod_Ref_Commons();
    }
  }
}

#ifndef KEY
//-----------------------------------------------------------------------
// NAME: Perform_Inline_Script_Analysis
// FUNCTION: Perform inlining analysis based on a context sensitive inlining specification file
//-----------------------------------------------------------------------
static void Perform_Inline_Script_Analysis(IPA_CALL_GRAPH* cg, MEM_POOL* pool, MEM_POOL* parser_pool)
{
    BOOL result = FALSE;
    IPA_NODE_ITER cg_iter (cg, LEVELORDER, pool);

#ifdef Enable_ISP_Verify // Additional debug information -- to be removed
    int null_caller_count = 0;
    int null_callee_count = 0;
#endif

    // traverse the call-graph, with visiting all nodes at levelorder first
    for (cg_iter.First(); !cg_iter.Is_Empty(); cg_iter.Next()) {
      IPA_NODE* caller = cg_iter.Current();
      if(caller) {
 	IPA_NODE_CONTEXT context (caller);
	cg->Map_Callsites (caller);
		
	IPA_SUCC_ITER edge_iter (cg, caller);
        for (edge_iter.First (); !edge_iter.Is_Empty (); edge_iter.Next ()) {
	    IPA_EDGE *edge = edge_iter.Current_Edge ();
            if (edge) {
                // Restore the WHIRL node information
            	IPA_NODE* callee = cg->Callee (edge);
    		WN* call_wn = edge->Whirl_Node();

    		// Retrieve the source line number, caller/callee file name and function name
    		INT32 callsite_linenum;
		USRCPOS callsite_srcpos;
    		char  *caller_filename, *callee_filename;
    		char  *caller_funcname, *callee_funcname;

    		IP_FILE_HDR& caller_hdr = caller->File_Header ();
    		IP_FILE_HDR& callee_hdr = callee->File_Header ();

    		if (call_wn == NULL) {
       			fprintf (stderr, "Warning: no source line number found for call-edge [%s --> %s]\n",
       	       			 caller->Name(), callee->Name());
       	  		callsite_linenum = 0;
    		}
  		else {
      			USRCPOS_srcpos(callsite_srcpos) = WN_Get_Linenum (call_wn);
      			callsite_linenum = USRCPOS_linenum(callsite_srcpos);
    		}

      		caller_filename = (char *) alloca(strlen(caller_hdr.file_name)+1);
		strcpy(caller_filename, caller_hdr.file_name);
		callee_filename = (char *) alloca(strlen(callee_hdr.file_name)+1);
		strcpy(callee_filename, callee_hdr.file_name);      		
      		
#ifdef Enable_ISP_Verify // Additional debug information -- to be removed
		fprintf (stderr, "Inline script analysis for call pair");
		fprintf (stderr, "(Name: %s, Line: %d, File: %s) -> callee (Name: %s, File: %s)\n",
         		caller->Name(), callsite_linenum, caller_filename,
         		callee->Name(), callee_filename);
#endif
    		
                // Assemble the caller_key and call_key for inquiry into the inlining record
    		char *caller_key, *callee_key;
    		ISP_Fix_Filename(caller_filename);
		caller_funcname = (char *) alloca(strlen(DEMANGLE (caller->Name()))+1);
		strcpy(caller_funcname, DEMANGLE (caller->Name()));    		
    		ISP_Fix_Filename(caller_funcname);
    		
    		caller_key = (char *) alloca(strlen(caller_filename)+strlen(caller_funcname)+2);
    		strcpy(caller_key, "");
    		strcat(caller_key, caller_filename);
    		strcat(caller_key, caller_funcname);

    		ISP_Fix_Filename(callee_filename);
		callee_funcname = (char *) alloca(strlen(DEMANGLE (callee->Name()))+1);
		strcpy(callee_funcname, DEMANGLE (callee->Name()));	    		
    		ISP_Fix_Filename(callee_funcname);
    		// Assumption: the line number of integer type should not exceed 30 digits (base-10)   		
    		char callsite_linestr[30];
    		sprintf(callsite_linestr, "%d", callsite_linenum);
    		
    		callee_key = (char *) alloca(strlen(callsite_linestr)+strlen(callee_filename)+strlen(callee_funcname)+3);
    		strcpy(callee_key, "");
    		strcat(callee_key, callsite_linestr);
    		strcat(callee_key, callee_filename);
    		strcat(callee_key, callee_funcname);

    		result = Check_Inline_Script(INLINE_Script_Name, caller_key, callee_key, parser_pool);
    		
    		// Set the call edge inlining attribute according to the inlining checking results
    		if(result == TRUE) {
    		    edge->Set_Must_Inline_Attrib();
    		} else {
    		    edge->Set_Noinline_Attrib();
    		}
            }
#ifdef Enable_ISP_Verify // Additional debug information -- to be removed
	    else null_callee_count++;
#endif	
	}
      }
#ifdef Enable_ISP_Verify // Additional debug information -- to be removed
      else null_caller_count++;
#endif
    }	

#ifdef Enable_ISP_Verify // Additional debug information -- to be removed
    fprintf (stderr, "Inline script DEBUG null_caller = %d, null_callee = %d\n", null_caller_count, null_callee_count);
#endif
#ifdef Enable_ISP_Verify
    Verify_Inline_Script();
#endif
}
#endif /* KEY */

extern void IPA_struct_opt_legality (void);

extern void IPA_identify_no_return_procs(void);

//-------------------------------------------------------------------------
// the main analysis phase at work! 
//-------------------------------------------------------------------------
void
Perform_Interprocedural_Analysis ()
{
    BOOL has_nested_pu = FALSE;
    BOOL run_autopar = FALSE;

    MEM_POOL_Popper pool (MEM_phase_nz_pool_ptr);

    if(IPA_Enable_Reorder)
		Init_merge_access();//field reorder

    // read PU infos, update summaries, and process globals
    for (UINT i = 0; i < IP_File_header.size(); ++i) {
      IPA_Process_File (IP_File_header[i]);
      if (IP_FILE_HDR_has_nested_pu(IP_File_header[i]))
	  has_nested_pu = TRUE;
      if (IP_FILE_HDR_file_header(IP_File_header[i])->Run_AutoPar())
          run_autopar = TRUE;
    }

    if ( Get_Trace ( TP_IPA,IPA_TRACE_TUNING_NEW ) && IPA_Enable_Reorder ) {
      fprintf ( TFile,
	       "\n%s%s\tstruct_access info after merging\n%s%s\n",
	       DBar, DBar, DBar, DBar );
      print_merged_access ();
    }

    if (run_autopar) {
#ifndef KEY
      // enable multi_cloning and preopt for parallelization analysis
      if (!IPA_Max_Node_Clones_Set) {
        IPA_Max_Node_Clones = 5; // default number of clones per PU
      }
      if (!IPA_Enable_Preopt_Set) {
        IPA_Enable_Preopt = TRUE;
      }
#endif // !KEY
    }
    else {
      // array section analysis is done only with -ipa -pfa
      IPA_Enable_Array_Sections = FALSE;
    }
    
    if (IPA_Enable_Padding || IPA_Enable_Split_Common) {
	Temporary_Error_Phase ephase ("IPA Padding Analysis");
	if (Verbose) {
	    fprintf (stderr, "Common blocks padding/split analysis ...");
	    fflush (stderr);
	}
	if (Trace_IPA || Trace_Perf)
	    fprintf (TFile, "\t<<<Padding/Split analysis begins>>>\n");
	Padding_Analysis (IP_File_header.size());
	if (Trace_IPA || Trace_Perf)
	    fprintf (TFile, "\t<<<Padding/Split analysis completed>>>\n");
    }

    // create and build a  call graph 
    {
	Temporary_Error_Phase ephase ("IPA Call Graph Construction");

        // Instantiate the Nystrom alias analyzer
        if (Alias_Nystrom_Analyzer)
          IPA_NystromAliasAnalyzer::create();

	if ( Get_Trace ( TKIND_ALLOC, TP_IPA ) ) {
	    fprintf ( TFile,
		      "\n%s%s\tMemory allocation information before Build_call_graph\n%s%s\n",
		      DBar, DBar, DBar, DBar );
	    MEM_Trace ();
	}
	
	if (Verbose) {
	    fprintf (stderr, "Building call graphs ...");
	    fflush (stderr);
	}
    
	if (Trace_IPA || Trace_Perf)
	    fprintf (TFile, "\t<<<Call Graph Construction begins>>>\n");
	
	Build_Call_Graph ();

	if(Get_Trace(TP_IPA, IPA_TRACE_TUNING)) // -tt19:0x40000
	{
  	  FILE *tmp_call_graph = fopen("cg_dump.log", "w");

	  if(tmp_call_graph != NULL)
	  {	  
	    fprintf(tmp_call_graph, "\t+++++++++++++++++++++++++++++++++++++++\n");
	    // KEY
  	    IPA_Call_Graph->Print_vobose(tmp_call_graph);
	    fprintf(tmp_call_graph, "\t+++++++++++++++++++++++++++++++++++++++\n");
	  }
	  fclose(tmp_call_graph);
	}

#ifdef KEY
        {
          IPA_NODE_ITER cg_iter(IPA_Call_Graph, POSTORDER);
	  // Traverse the call graph and mark C++ nodes as PU_Can_Throw
	  // appropriately.
          for (cg_iter.First(); !cg_iter.Is_Empty(); cg_iter.Next())
	  {
	    if (!IPA_Enable_EH_Region_Removal && !IPA_Enable_Pure_Call_Opt)
	    	break;

	    IPA_NODE * node = cg_iter.Current();
	    if (!node)	continue;

	    if (node->PU_Can_Throw() ||
	        node->Summary_Proc()->Has_side_effect())
	    { // Mark its callers appropriately
	    	IPA_PRED_ITER preds (node->Node_Index());
		for (preds.First(); !preds.Is_Empty(); preds.Next())
		{
		    IPA_EDGE * edge = preds.Current_Edge();
		    if (edge)
		    {
			IPA_NODE * caller = IPA_Call_Graph->Caller (edge);

			PU caller_pu = Pu_Table[ST_pu((caller)->Func_ST())];
			if (IPA_Enable_EH_Region_Removal &&
			    node->PU_Can_Throw() &&
			    PU_src_lang (caller_pu) & PU_CXX_LANG)
		    	  caller->Set_PU_Can_Throw();

		        if (node->Summary_Proc()->Has_side_effect())
			  caller->Summary_Proc()->Set_has_side_effect();
		    }
		}
	    }
	  }

    	  if (IPA_Enable_Source_PU_Order || Opt_Options_Inconsistent)
	    for (UINT i = 0; i < IP_File_header.size(); ++i)
	    { // Store the file-id in each IPA_NODE
	      Mark_PUs_With_File_Id (IP_FILE_HDR_pu_list (IP_File_header[i]), i);
	    }
        }
#endif

	// INLINING TOOL
	if (INLINE_Enable_Script) {
#ifdef KEY
	        fprintf (stdout, "inline script not implemented\n");
		exit (1);
#else
	        MEM_POOL script_parser_pool;
	        MEM_POOL_Initialize(&script_parser_pool, "inlining script parser pool", FALSE);
	        MEM_POOL_Push(&script_parser_pool);
	        	
		MEM_POOL_Popper inline_script_pool (MEM_local_nz_pool_ptr);
		Perform_Inline_Script_Analysis(IPA_Call_Graph, inline_script_pool.Pool(), &script_parser_pool);
		
		MEM_POOL_Pop(&script_parser_pool);
		MEM_POOL_Delete(&script_parser_pool);
#endif /* KEY */
	}
        // INLINING TOOL END
	
	if (has_nested_pu) {
	    Build_Nested_Pu_Relations();
	    if (Verbose) {
		fprintf (stderr, "Building Nested PU Relations...");
		fflush (stderr);
	    }
	}
    }
    
#ifdef Is_True_On
    CGB_IPA_Initialize(IPA_Call_Graph);
#endif

    Ipa_tlog( "Must Inline", 0, "Count %d", Total_Must_Inlined);
    Ipa_tlog( "Must Not-Inline", 0, "Count %d", Total_Must_Not_Inlined);

    if (Trace_IPA || Trace_Perf)
	fprintf (TFile, "\t<<<Call Graph Construction completed>>>\n");

#ifdef TODO
    if (IPA_Enable_daVinci) {
	cg_display = (daVinci *)
	    CXX_NEW (daVinci (IPA_Call_Graph->Graph (), Malloc_Mem_Pool),
		     Malloc_Mem_Pool); 

	cg_display->Translate_Call_Graph ();
    }
#endif // TODO
    
    if ( Get_Trace ( TKIND_ALLOC, TP_IPA ) ) {
      fprintf ( TFile,
	       "\n%s%s\tMemory allocation information after "
	       "Build_call_graph\n%s%s\n",
	       DBar, DBar, DBar, DBar );
      MEM_Trace ();
    }

#ifdef DRAGON
    if(Dragon_Flag)
    {
        int len = strlen (output_filename);
        char *cg_filename = new char[len+3];
        strcpy(cg_filename,output_filename);
        strcat(cg_filename,".dgn");

        char *rg_filename = new char[len+7];
        strcpy(rg_filename,output_filename);
        strcat(rg_filename,".rgn");

        Dragon_Dump_Array_Regions(rg_filename);

        ofstream fout(cg_filename,ios::out | ios::binary);
        IPA_Call_Graph->Dragon_Print(fout);
        //Debug ,Liao
        // FILE *myFp= fopen("cfg.dump","w");
        //IPA_Call_Graph->Print_vobose(myFp);
        //fclose(myFp);
        fout.close();
        delete [] cg_filename;
        delete [] rg_filename;
        cleanup_all_files();
        //     Signal_Cleanup(1);

        if (Verbose) {
          fprintf (stderr, "\n");
        }

        fprintf (stderr, "Dragon call graph created. Exiting. \n");
        fflush (stderr);
        exit(0);
    }
#endif

    // This is where we do the partitioning and looping through the
    // different partition to do the analysis

    if ((IPA_Enable_SP_Partition && (IPA_Space_Access_Mode == SAVE_SPACE_MODE))
	|| IPA_Enable_GP_Partition) {
        // This is where we do the partitioning and build the partitioning
	// data structure if necessary
	// In the partitioning algorithm, it should for each IPA_NODE
	// 1) tag with a partition group, using Set_partition_group()
	// 2) whether it is INTERNAL to the partition using Set_partition_internal()
	// then for each DEFINED external DATA symbol,
	// 1) tag whether it is INTERNAL
	// 2) whether it is gp-relative
	//
	// to that partition.  The partitioning algorithm should take
	// into account whether it is doing partitioning for
	// solving 1) space problem(IPA_Enable_SP_Partition)
	//  or 2) multigot problem(IPA_Enable_GP_Partition)
	// 
	// Also, something new:
	// pobj->ipa_info.gp_status & pobj->ipa_info.partition_grp needs
	// to be set here for each object being looked at also
    }

    // need by ipa_inline
    Total_Prog_Size = Orig_Prog_Weight;

    MEM_POOL_Push (MEM_local_nz_pool_ptr);

    if (IPA_Enable_DVE || IPA_Enable_CGI) {
	Temporary_Error_Phase ephase ("IPA Global Variable Optimization");
	if (Verbose) {
	    fprintf (stderr, "Global Variable Optimization ...");
	    fflush (stderr);
	}
	if (Trace_IPA || Trace_Perf)
	    fprintf (TFile, "\t<<<Global Variable Optimization begins>>>\n");
#ifdef TODO
        if( IPA_Enable_Feedback ) {
                setup_IPA_feedback_phase();
	    fprintf(IPA_Feedback_dve_fd,
                        "\nDEAD VARIABLES (defined but not used)\n\n");
        }
#endif
	extern void Optimize_Global_Variables ();
	Optimize_Global_Variables ();                                              
	
	if (Trace_IPA || Trace_Perf)
	    fprintf  (TFile,
		      "\t<<<Global Variable Optimization completed>>>\n");
#ifdef TODO
        if( IPA_Enable_Feedback ) {
                cleanup_IPA_feedback_phase ();
                fflush(IPA_Feedback_dve_fd);
        }
#endif
    }

    if(IPA_Enable_Reorder && !merged_access->empty())
		IPA_reorder_legality_process(); 	

#ifdef KEY
    if (IPA_Enable_Struct_Opt)
        IPA_struct_opt_legality();
#endif

    //  mark all unreachable nodes that are either EXPORT_LOCAL (file
    //  static) or EXPORT_INTERNAL *AND* do not have address taken as
    // "deletable".  Functions that are completely inlined to their
    //  callers are taken care of later.
    if (IPA_Enable_DFE) {
	Temporary_Error_Phase ephase ("IPA Dead Functions Elimination");
	if (Verbose) {
	    fprintf (stderr, "Dead functions elimination ...");
	    fflush (stderr);
	}
#ifdef TODO
        if( IPA_Enable_Feedback ) {
            //
            // Functions completely inlined to their callers are handled in
            // routine Perform_inlining (ipa_inline.cxx) and are not marked
            // deletable until then. Thus the dfe info presented here does
            // represent truly dead source.
            //
            setup_IPA_feedback_phase();
	    fprintf(IPA_Feedback_dfe_fd,
                        "\nDEAD FUNCTIONS (but not due to inlining)\n\n");
        }
#endif
	if (Trace_IPA || Trace_Perf)
	    fprintf (TFile, "\t<<<Dead Functions Elimination begins>>>\n");
	Total_Dead_Function_Weight = Eliminate_Dead_Func ();
	Total_Prog_Size = Orig_Prog_Weight - Total_Dead_Function_Weight;
	if (Trace_IPA || Trace_Perf)
	     fprintf (TFile, "\t<<<Dead Functions Elimination completed>>>\n");
#ifdef TODO
        if( IPA_Enable_Feedback ) {
                cleanup_IPA_feedback_phase ();
                fflush(IPA_Feedback_dfe_fd);
        }
#endif
    }

/*
on virtual function optimization pass:
The virtual function optimization pass is invoked here 
in ipa_main.cxx Perform_Interprocedural_Analysis function after
Build_Call_Graph.
this is the psuedo code that describes where the optimization must be 
placed.
Perform_Interprocedural_Analysis() { // ipa/main/analyze/ipa_main.cxx
    ... // Need to have built the call graph prior to my pass
    Build_Call_Graph ();
    ...
        // Note 1: We need a call graph prior to making this function call
        // Note 2: Uncommenting the following function: IPA_fast_static_analysis_VF
        // may lead to unknown behavior.
        // if you want to disable virtual function optimization,
        // use the BOOL variable in config/config_ipa.cxx,
        // IPA_Enable_fast_static_analysis_VF. Set it to FALSE to disable 
        // the pass.
    IPA_fast_static_analysis_VF () ; //  ipa/main/analyze/ipa_devirtual.cxx 

    ...
}
*/

#if defined(KEY) && !defined(_STANDALONE_INLINER) && !defined(_LIGHTWEIGHT_INLINER)
    if (IPA_Enable_Fast_Static_Analysis_VF == TRUE) {
        IPA_Fast_Static_Analysis_VF ();
    }
#endif // KEY && !(_STANDALONE_INLINER) && !(_LIGHTWEIGHT_INLINER)
    // Devirtualization using IPA_Enable_Devirtualization enabled path is not used from open64 4.2.2-1. Please use IPA_Enable_Fast_Static_Analysis_VF enabled path for understanding devirtualization.

    if (IPA_Enable_Devirtualization) { 
        Temporary_Error_Phase ephase ("IPA Devirtualization"); 
        IPA_Class_Hierarchy = Build_Class_Hierarchy(); 
        IPA_devirtualization(); 
    } 

    if ( IPA_Enable_Simple_Alias ) {
      Temporary_Error_Phase ephase ("Interprocedural Alias Analysis");
      if (Verbose) {
	  fprintf (stderr, "Alias analysis ...");
	  fflush (stderr);
      }

      IPA_NystromAliasAnalyzer *ipa_naa =
                                    IPA_NystromAliasAnalyzer::aliasAnalyzer();
      if (ipa_naa) {
        ipa_naa->solver(IPA_Call_Graph);
      }

      IPAA ipaa(NULL);
      ipaa.Do_Simple_IPAA ( *IPA_Call_Graph );

      if ( Get_Trace ( TKIND_ALLOC, TP_IPA ) ) {
	fprintf ( TFile,
		 "\n%s%s\tMemory allocation information after IPAA\n%s%s\n",
		 DBar, DBar, DBar, DBar );
	MEM_Trace ();
      }
    }
    else {
      // Common block constants and aggresive node cloning
      // can only be done when alias information is available
      IPA_Enable_Cprop = FALSE;
      IPA_Enable_Common_Const = FALSE;
      IPA_Max_Node_Clones = 0;
    }

    // Propagate information about formal parameters used as 
    // symbolic terms in array section summaries.
    // This information will later be used to trigger cloning.
    if (IPA_Enable_Array_Sections &&
        IPA_Max_Node_Clones > 0   && 
        IPA_Max_Clone_Bloat > 0) {

      Temporary_Error_Phase ephase ("IPA Cloning Analysis");
      if (Verbose) {
        fprintf (stderr, "Cloning Analysis ...");
        fflush (stderr);
      }
      if (Trace_IPA || Trace_Perf) {
	fprintf (TFile, "\t<<<Analysis of formals for cloning begins>>>\n");
      }

      IPA_FORMALS_IN_ARRAY_SECTION_DF clone_df(IPA_Call_Graph, 
                                               BACKWARD,
                                               MEM_local_nz_pool_ptr);
      clone_df.Init();
      clone_df.Solve();

      if (Get_Trace(TP_IPA, IPA_TRACE_CPROP_CLONING)) {
        clone_df.Print(TFile);
      }
      if (Trace_IPA || Trace_Perf) {
	fprintf (TFile, "\t<<<Analysis of formals for cloning ends>>>\n");
      }
    }


    // solve interprocedural constant propagation     
    if (IPA_Enable_Cprop) {
      Temporary_Error_Phase ephase ("IPA Constant Propagation");

      if ( Get_Trace ( TKIND_ALLOC, TP_IPA ) ) {
	fprintf ( TFile,
		 "\n%s%s\tMemory allocation information before IP constant propagation\n%s%s\n",
		 DBar, DBar, DBar, DBar );
	MEM_Trace ();
      }

      if (Verbose) {
	fprintf (stderr, "Constant propagation ...");
	fflush (stderr);
      }
      if (Trace_IPA || Trace_Perf)
	fprintf (TFile, "\t<<<Constant Propagation begins>>>\n");

      MEM_POOL_Initialize (&Ipa_cprop_pool, "cprop pool", 0);

      // Set the upper limit for the total number of clone nodes 
      IPA_Max_Total_Clones = 
	(GRAPH_vcnt(IPA_Call_Graph->Graph()) * IPA_Max_Clone_Bloat) / 100;

      if (IPA_Enable_Common_Const) {
        static BOOL global_cprop_pool_inited = FALSE;
        if (!global_cprop_pool_inited) {
          MEM_POOL_Initialize(&Global_mem_pool, "global_cprop_mem_pool", 0);
          MEM_POOL_Push (&Global_mem_pool);
          global_cprop_pool_inited = TRUE;
        }
	MEM_POOL_Initialize(&local_cprop_pool, "local_cprop_mem_pool", 0);
	MEM_POOL_Push(&local_cprop_pool);
      }

      IPA_CPROP_DF_FLOW df (FORWARD, MEM_local_nz_pool_ptr);

      df.Init();   // initialize the annotations
      df.Solve();  // solve the data flow problem


      // Convert quasi clones into real ones
      IPA_NODE_ITER cg_iter(IPA_Call_Graph, POSTORDER);
      for (cg_iter.First(); !cg_iter.Is_Empty(); cg_iter.Next()) {
        IPA_NODE* node = cg_iter.Current();
        if (node && node->Is_Quasi_Clone()) {
          IPA_Call_Graph->Quasi_To_Real_Clone(node);
        }
      }
      if (Get_Trace(TP_IPA, IPA_TRACE_CPROP_CLONING)) {
        IPA_Call_Graph->Print(TFile);
      }
    
    
      if (IPA_Enable_Common_Const) {
        MEM_POOL_Pop(&local_cprop_pool);
        MEM_POOL_Delete(&local_cprop_pool);
      }
	
      // in the process perform cloning
      if (Trace_IPA || Trace_Perf) {
        df.Print(TFile);
        fprintf(TFile, "Constant Count = %d \n", IPA_Constant_Count);
        fprintf (TFile,"\t<<<Constant Propagation ends>>>\n");
      }
      Ipa_tlog( "Cprop", 0, "Count %d", IPA_Constant_Count);

#ifdef TODO
      // check for IPA:feedback=ON - get constant info if so
      if( IPA_Enable_Feedback ) {
        fprintf(IPA_Feedback_con_fd,"\nCONSTANTS FOUND\n\n");
        df.Print(IPA_Feedback_con_fd);
        fflush(IPA_Feedback_con_fd);
      }
#endif // TODO

      if (WN_mem_pool_ptr == &Ipa_cprop_pool) 
	WN_mem_pool_ptr = NULL;
      MEM_POOL_Delete (&Ipa_cprop_pool);

      if ( Get_Trace ( TKIND_ALLOC, TP_IPA ) ) {
        fprintf ( TFile,
                  "\n%s%s\tMemory allocation information after IP constant propagation\n%s%s\n",
                  DBar, DBar, DBar, DBar );
        MEM_Trace ();
      }
    }

#ifdef KEY
    if (IPA_Enable_Preopt)
      Preprocess_struct_access();
#endif // KEY

#ifdef OPENSHMEM_ANALYZER
    if (OSA_Flag) {
      IPA_OpenSHMEM_Check(IPA_Call_Graph);
    }
#endif

    // Call preopt on each node if requested
    if (IPA_Enable_Preopt_Set && IPA_Enable_Preopt) {
      IPA_NODE_ITER cg_iter(IPA_Call_Graph, POSTORDER);
      for (cg_iter.First(); !cg_iter.Is_Empty(); cg_iter.Next()) {
        if (IPA_NODE* node = cg_iter.Current()) {
          IPA_Preoptimize(node);
        }
      }
    }

    if (IPA_Enable_Siloed_Ref) {
    	IPA_Concurrency_Graph = CXX_NEW(IPA_PCG(IPA_Call_Graph, Malloc_Mem_Pool),
    			Malloc_Mem_Pool);
    	IPA_Concurrency_Graph->Collect_siloed_references();
    }

    MEM_POOL_Pop (MEM_local_nz_pool_ptr);

    // solve interprocedural array section analysis
    if (IPA_Enable_Array_Sections) {

      Temporary_Error_Phase ephase ("IPA Array Section Analysis");
      if ( Get_Trace ( TKIND_ALLOC, TP_IPA ) ) {
	fprintf ( TFile,
		 "\n%s%s\tMemory allocation information before IP array section propagation \n%s%s\n", DBar, DBar, DBar, DBar );
	MEM_Trace ();
      }
      if (Verbose) {
	fprintf (stderr, "Array Section analysis ...");
	fflush (stderr);
      }
      if (Trace_IPA || Trace_Perf) {
	fprintf (TFile, "\t<<<Array section propagation begins>>>\n");
      }
      
      MEM_POOL_Push (MEM_local_nz_pool_ptr);

      IPA_ARRAY_DF_FLOW array_df (IPA_Call_Graph, 
                                  BACKWARD, 
                                  MEM_local_nz_pool_ptr);

      array_df.Init();   // initialize the annotations
      Print_Array_Sections("BEFORE PROPAGATION:");

      array_df.Solve();  // solve the data flow problem
      Print_Array_Sections("AFTER PROPAGATION:");

      if (Trace_IPA || Trace_Perf) {
	fprintf (TFile,"\t<<<Array section propagation ends>>>\n");
      }

      MEM_POOL_Pop (MEM_local_nz_pool_ptr);

      if ( Get_Trace ( TKIND_ALLOC, TP_IPA ) ) {
	fprintf ( TFile,
               "\n%s%s\tMemory allocation information after IP array section propagation \n%s%s\n",
               DBar, DBar, DBar, DBar );
	MEM_Trace ();
      }
    }

    if (IPA_Enable_Preopt) {
      IPA_Preopt_Finalize();
    }
 
    // solve interprocedural inlining
    if (IPA_Enable_Inline || IPA_Enable_DCE) {

	MEM_POOL_Popper inline_pool (MEM_local_nz_pool_ptr);
	
        if (Verbose) {
            fprintf (stderr, "Inlining analysis ...");
            fflush (stderr);
        }

	Temporary_Error_Phase ephase ("IPA Inlining Analysis");
	if (Trace_IPA || Trace_Perf)
	    fprintf (TFile, "\t<<<Inlining analysis begins>>>\n");
#ifdef TODO
        if( IPA_Enable_Feedback ) {
            setup_IPA_feedback_phase();
            fprintf(IPA_Feedback_prg_fd,"\nINLINING FAILURE INFO\n\n");
        }
#endif
	Perform_Inline_Analysis (IPA_Call_Graph, inline_pool.Pool());

	if (Trace_IPA || Trace_Perf) {
	    fprintf (TFile, "\n\tTotal code expansion = %d%%, total prog WHIRL size = 0x%x \n",
		     Orig_Prog_Weight == 0 ? 0 : (Total_Prog_Size - (INT) Orig_Prog_Weight) * 100 / (INT) Orig_Prog_Weight,
		     Total_Prog_Size);
	    fprintf (TFile, "\t<<<Inlining analysis completed>>>\n");
	}
#ifdef TODO
        if( IPA_Enable_Feedback ) {
                cleanup_IPA_feedback_phase ();
                fflush(IPA_Feedback_prg_fd);
        }
#endif // TODO

	Ipa_tlog( "Inline", 0, "Count %d", Total_Inlined);
	Ipa_tlog( "Not-Inline", 0, "Count %d", Total_Not_Inlined);
    }

    /* print the call graph */
#ifdef Is_True_On
    CGB_IPA_Terminate();
#endif

   IPA_identify_no_return_procs();

   if (Verbose) {
     fprintf (stderr, "\n");
     fflush (stderr);
   }
}

#ifdef OPENSHMEM_ANALYZER

int IPA_IsOpenSHMEM(char *);

void IPA_OpenSHMEM_Check(IPA_CALL_GRAPH* cg)
{
    int debug=0;
    if(debug)
        //printf("\n ***Entering OpenSHMEM Checker**** \n");
        if(debug)
            cg->Print_vobose(stdout);
    cg->OpenSHMEM_Init_Checks(TRUE);
    cg->OpenSHMEM_IO_Checks();
    if(debug) printf("\n*** Exiting OpenSHMEM IO checks ***");
    fclose(makefile);
    cleanup_all_files();
    // remove_temp_dir();

#ifndef _LIGHTWEIGHT_INLINER
    int pid = fork();
    if(pid == 0) {
        execlp("callgraph","callgraph",output_filename,NULL);
        printf("\n**** OpenSHMEM Analyzer was not able to create the "
               "OpenSHMEM callgraph. Check if the callgraph executable "
               "is in your $PATH variable ****\n");
        exit(0);
    }
#endif
    //  cg_debug();

    //   Signal_Cleanup (0);
    exit(0);
}

int IPA_IsOpenSHMEM(char *input)
{
    int debug=1;
    char shmem_name[190][50] ={
        /*
         * Initialization & rtl  // 0
         */
        "first_name",
        "start_pes",   // 1
        "shmem_init",
        "shmem_finalize",
        "shmem_my_pe",  // 4
        "my_pe",
        "_my_pe",
        "shmem_num_pes",
        "shmem_n_pes",
        "num_pes",
        "_num_pes",
        "shmem_nodename",
        "shmem_version",
        /*
         * I/O  // 13
         */
        "shmem_short_put",
        "shmem_int_put",
        "shmem_long_put",
        "shmem_longlong_put",
        "shmem_longdouble_put",
        "shmem_double_put",
        "shmem_float_put",
        "shmem_putmem",
        "shmem_put32",
        "shmem_put64",
        "shmem_put128",

        // 24

        "shmem_short_get",
        "shmem_int_get",
        "shmem_long_get",
        "shmem_longlong_get",
        "shmem_longdouble_get",
        "shmem_double_get",
        "shmem_float_get",
        "shmem_getmem",
        "shmem_get32",
        "shmem_get64",
        "shmem_get128",
        // 35
        "shmem_char_p",
        "shmem_short_p",
        "shmem_int_p",
        "shmem_long_p",
        "shmem_longlong_p",
        "shmem_float_p",
        "shmem_double_p",
        "shmem_longdouble_p",
        //43
        "shmem_char_g",
        "shmem_short_g",
        "shmem_int_g",
        "shmem_long_g",
        "shmem_longlong_g",
        "shmem_float_g",
        "shmem_double_g",
        "shmem_longdouble_g",

        /*
         * non-blocking I/O
         */  //51
        "shmem_short_put_nb",
        "shmem_int_put_nb",
        "shmem_long_put_nb",
        "shmem_longlong_put_nb",
        "shmem_longdouble_put_nb",
        "shmem_double_put_nb",
        "shmem_float_put_nb",
        "shmem_putmem_nb",
        "shmem_put32_nb",
        "shmem_put64_nb",
        "shmem_put128_nb",
        /*
         * strided I/O
         */  //62
        "shmem_double_iput",
        "shmem_float_iput",
        "shmem_int_iput",
        "shmem_iput32",
        "shmem_iput64",
        "shmem_iput128",
        "shmem_long_iput",
        "shmem_longdouble_iput",
        "shmem_longlong_iput",
        "shmem_short_iput",
        //72
        "shmem_double_iget",
        "shmem_float_iget",
        "shmem_int_iget",
        "shmem_iget32",
        "shmem_iget64",
        "shmem_iget128",
        "shmem_long_iget",
        "shmem_longdouble_iget",
        "shmem_longlong_iget",
        "shmem_short_iget",
        /*
         * barriers
         */ //82
        "shmem_barrier_all",
        "shmem_barrier",
        "shmem_fence",
        "shmem_quiet",
        /*
         * accessibility
         */  //86
        "shmem_pe_accessible",
        "shmem_addr_accessible",
        /*
         * symmetric memory management
         */  // 88
        "shmalloc",
        "shfree",
        "shrealloc",
        "shmemalign",
        "shmem_malloc",
        "shmem_free",
        "shmem_realloc",
        "shmem_memalign",
        "sherror",
        "shmem_error",
        /*
         * wait operations
         *///98
        "shmem_short_wait_until",
        "shmem_int_wait_until",
        "shmem_long_wait_until",
        "shmem_longlong_wait_until",
        "shmem_wait_until",
        "shmem_short_wait",
        "shmem_int_wait",
        "shmem_long_wait",
        "shmem_longlong_wait",
        "shmem_wait",
        /*
         * atomic swaps
         *///108
        "shmem_int_swap",
        "shmem_long_swap",
        "shmem_longlong_swap",
        "shmem_float_swap",
        "shmem_double_swap",
        "shmem_swap",
        "shmem_int_cswap",
        "shmem_long_cswap",
        "shmem_longlong_cswap",
        /*
         * atomic fetch-{add,inc} & add,inc
         */
        //117
        "shmem_int_fadd",
        "shmem_long_fadd",
        "shmem_longlong_fadd",
        "shmem_int_finc",
        "shmem_long_finc",
        "shmem_longlong_finc",
        "shmem_int_add",
        "shmem_long_add",
        "shmem_longlong_add",
        "shmem_int_inc",
        "shmem_long_inc",
        "shmem_longlong_inc",
        /*
         * cache flushing
         *///129
        "shmem_clear_cache_inv",
        "shmem_set_cache_inv",
        "shmem_clear_cache_line_inv",
        "shmem_set_cache_line_inv",
        "shmem_udcflush",
        "shmem_udcflush_line",
        /*
         * reductions
         */
        //135
        "shmem_complexd_sum_to_all",
        "shmem_complexf_sum_to_all",
        "shmem_double_sum_to_all",
        "shmem_float_sum_to_all",
        "shmem_int_sum_to_all",
        "shmem_long_sum_to_all",
        "shmem_longdouble_sum_to_all",
        "shmem_longlong_sum_to_all",
        "shmem_short_sum_to_all",
        "shmem_complexd_prod_to_all",
        "shmem_complexf_prod_to_all",
        "shmem_double_prod_to_all",
        "shmem_float_prod_to_all",
        "shmem_int_prod_to_all",
        "shmem_long_prod_to_all",
        "shmem_longdouble_prod_to_all",
        "shmem_longlong_prod_to_all",
        "shmem_short_prod_to_all",
        "shmem_int_and_to_all",
        "shmem_long_and_to_all",
        "shmem_longlong_and_to_all",
        "shmem_short_and_to_all",
        "shmem_int_or_to_all",
        "shmem_long_or_to_all",
        "shmem_longlong_or_to_all",
        "shmem_short_or_to_all",
        "shmem_int_xor_to_all",
        "shmem_long_xor_to_all",
        "shmem_longlong_xor_to_all",
        "shmem_short_xor_to_all",
        "shmem_int_max_to_all",
        "shmem_long_max_to_all",
        "shmem_longlong_max_to_all",
        "shmem_short_max_to_all",
        "shmem_longdouble_max_to_all",
        "shmem_float_max_to_all",
        "shmem_double_max_to_all",
        "shmem_int_min_to_all",
        "shmem_long_min_to_all",
        "shmem_longlong_min_to_all",
        "shmem_short_min_to_all",
        "shmem_longdouble_min_to_all",
        "shmem_float_min_to_all",
        "shmem_double_min_to_all",
        /*
         * broadcasts
         */
        //179
        "shmem_broadcast32",
        "shmem_broadcast64",
        "shmem_sync_init",
        /*
         * collects
         */
        //182
        "shmem_fcollect32",
        "shmem_fcollect64",
        "shmem_collect32",
        "shmem_collect64",
        /*
         * locks/critical section
         */
        //186
        "shmem_set_lock",
        "shmem_clear_lock",
        "shmem_test_lock"
    };
    for(int i=1;i<189;i++) {
        if(strcmp(shmem_name[i],input)==0) {
            if(debug) printf("\n*** OpenSHMEM call: %s ***\n", shmem_name[i]);
            return 1;

        }

    }
    if(debug)
        printf("\nThis is the last SHMEM call: %s\n", shmem_name[188]);

    return 0;
}

#endif /* defined(OPENSHMEM_ANALYZER) */
