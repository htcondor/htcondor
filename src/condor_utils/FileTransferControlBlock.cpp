#include "condor_common.h"
#include "condor_debug.h"

#include <string>
#include "condor_classad.h"
#include "FileTransferControlBlock.h"

#include "condor_attributes.h"


FileTransferControlBlock::FileTransferControlBlock() {
}


FileTransferControlBlock::FileTransferControlBlock( ClassAd * jobAd ) {
	jobAd->LookupBool( ATTR_STREAM_OUTPUT, STREAM_OUTPUT );
	jobAd->LookupBool( ATTR_STREAM_ERROR, STREAM_ERROR );
	jobAd->LookupBool( ATTR_PRESERVE_RELATIVE_PATHS, PRESERVE_RELATIVE_PATHS );
	jobAd->LookupBool( ATTR_TRANSFER_EXECUTABLE, TRANSFER_EXECUTABLE );

	jobAd->LookupString( ATTR_CONTAINER_IMAGE, CONTAINER_IMAGE );
	jobAd->LookupString( ATTR_JOB_CMD, JOB_CMD );
	jobAd->LookupString( ATTR_GLOBAL_JOB_ID, GLOBAL_JOB_ID );
	jobAd->LookupString( ATTR_JOB_ORIGINAL_OUTPUT, JOB_ORIGINAL_OUTPUT );
	jobAd->LookupString( ATTR_JOB_ORIGINAL_ERROR, JOB_ORIGINAL_ERROR );

	jobAd->LookupInteger( ATTR_CLUSTER_ID, CLUSTER_ID );
	jobAd->LookupInteger( ATTR_PROC_ID, PROC_ID );
	jobAd->LookupInteger( ATTR_STAGE_IN_FINISH, STAGE_IN_FINISH );


	eval_OutputDirectory = jobAd->LookupString(
		"OutputDirectory", OutputDirectory
	);
	eval_TRANSFER_OUTPUT_REMAPS = jobAd->EvaluateAttrString(
		ATTR_TRANSFER_OUTPUT_REMAPS, TRANSFER_OUTPUT_REMAPS
	);
	eval_USER = jobAd->EvaluateAttrString( ATTR_USER, USER );
	eval_DataReuseManifestSHA256 = jobAd->EvaluateAttrString(
		"DataReuseManifestSHA256", DataReuseManifestSHA256
	);
	eval_JOB_IWD = jobAd->EvaluateAttrString( ATTR_JOB_IWD, JOB_IWD );
	eval_OWNER = jobAd->LookupString( ATTR_OWNER, OWNER );
	eval_TRANSFER_INPUT_FILES = jobAd->LookupString(
		ATTR_TRANSFER_INPUT_FILES, TRANSFER_INPUT_FILES
	);
	eval_ULOG_FILE = jobAd->LookupString( ATTR_ULOG_FILE, ULOG_FILE );
	eval_TRANSFER_KEY = jobAd->LookupString( ATTR_TRANSFER_KEY, TRANSFER_KEY );
	eval_TRANSFER_SOCKET = jobAd->LookupString(
		ATTR_TRANSFER_SOCKET, TRANSFER_SOCKET
	);

	eval_TRANSFER_INTERMEDIATE_FILES = jobAd->LookupString(
		ATTR_TRANSFER_INTERMEDIATE_FILES, TRANSFER_INTERMEDIATE_FILES
	);
	eval_X509_USER_PROXY = jobAd->LookupString(
		ATTR_X509_USER_PROXY, X509_USER_PROXY
	);
	eval_CHECKPOINT_FILES = jobAd->LookupString(
		ATTR_CHECKPOINT_FILES, CHECKPOINT_FILES
	);
	eval_JOB_CHECKPOINT_DESTINATION = jobAd->LookupString(
		ATTR_JOB_CHECKPOINT_DESTINATION, JOB_CHECKPOINT_DESTINATION
	);
	eval_PUBLIC_INPUT_FILES = jobAd->LookupString(
		ATTR_PUBLIC_INPUT_FILES, PUBLIC_INPUT_FILES
	);
	eval_JOB_INPUT = jobAd->LookupString(
		ATTR_JOB_INPUT, JOB_INPUT
	);
	eval_OUTPUT_DESTINATION = jobAd->LookupString(
		ATTR_OUTPUT_DESTINATION, OUTPUT_DESTINATION
	);
	eval_JOB_ORIG_CMD = jobAd->LookupString(
		ATTR_JOB_ORIG_CMD, JOB_ORIG_CMD
	);
	eval_SPOOLED_OUTPUT_FILES = jobAd->LookupString(
		ATTR_SPOOLED_OUTPUT_FILES, SPOOLED_OUTPUT_FILES
	);
	eval_TRANSFER_OUTPUT_FILES = jobAd->LookupString(
		ATTR_TRANSFER_OUTPUT_FILES, TRANSFER_OUTPUT_FILES
	);
	eval_JOB_OUTPUT = jobAd->LookupString(
		ATTR_JOB_OUTPUT, JOB_OUTPUT
	);
	eval_JOB_ERROR = jobAd->LookupString(
		ATTR_JOB_ERROR, JOB_ERROR
	);
	eval_JOB_INPUT = jobAd->LookupString(
		ATTR_JOB_INPUT, JOB_INPUT
	);
	eval_ENCRYPT_INPUT_FILES = jobAd->LookupString(
		ATTR_ENCRYPT_INPUT_FILES, ENCRYPT_INPUT_FILES
	);
	eval_ENCRYPT_OUTPUT_FILES = jobAd->LookupString(
		ATTR_ENCRYPT_OUTPUT_FILES, ENCRYPT_OUTPUT_FILES
	);
	eval_DONT_ENCRYPT_INPUT_FILES = jobAd->LookupString(
		ATTR_DONT_ENCRYPT_INPUT_FILES, DONT_ENCRYPT_INPUT_FILES
	);
	eval_DONT_ENCRYPT_OUTPUT_FILES = jobAd->LookupString(
		ATTR_DONT_ENCRYPT_OUTPUT_FILES, DONT_ENCRYPT_OUTPUT_FILES
	);
	eval_FAILURE_FILES = jobAd->LookupString(
		ATTR_FAILURE_FILES, FAILURE_FILES
	);


	ExprTree * tree = jobAd->Lookup(ATTR_TRANSFER_Q_URL_IN_LIST);
	if( tree ) {
		eval_TRANSFER_Q_URL_IN_LIST = true;
		if( tree->GetKind() == ClassAd::ExprTree::EXPR_LIST_NODE ) {
			classad::ExprList * list = dynamic_cast<classad::ExprList *>(tree);
			if( list != NULL ) {
				list_TRANSFER_Q_URL_IN_LIST = true;
				TRANSFER_Q_URL_IN_LIST.CopyFrom( * list );
			}
		}
	}
}
