 ###############################################################
 # 
 # Copyright 2011 Red Hat, Inc. 
 # 
 # Licensed under the Apache License, Version 2.0 (the "License"); you 
 # may not use this file except in compliance with the License.  You may 
 # obtain a copy of the License at 
 # 
 #    http://www.apache.org/licenses/LICENSE-2.0 
 # 
 # Unless required by applicable law or agreed to in writing, software 
 # distributed under the License is distributed on an "AS IS" BASIS, 
 # WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 # See the License for the specific language governing permissions and 
 # limitations under the License. 
 # 
 ############################################################### 

if( STD_UNIVERSE )

	add_definitions( -DSKIP_AUTHENTICATION -DHEADER_DES_LOCL_H )
	condor_glob( stdiohdrs stdiosrcs "" )
	condor_static_lib(stdunivio "${stdiohdrs};${stdiosrcs}")

endif( STD_UNIVERSE )
