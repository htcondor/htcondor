/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#include "classad_distribution.h"
#include "query.h"
#include "view.h"
#include <iostream>
using std::string;
using std::vector;
using std::pair;

namespace classad {

ClassAdCollection server(true);
/*******************************************************
 Function:
   Display all the ClassAd in the Collection
 *******************************************************/
void dump_collection(){

  cout << " here are what we have in Collection ! " << endl;
  string q_key;
  ClassAd *tmp;
  LocalCollectionQuery a;
  a.Bind(&server);
  a.Query("root",NULL);
  a.ToFirst();
  bool ret=a.Current(q_key);
  if (ret==true){
    do{
            cout << "q_key " << q_key <<endl;
	    tmp=server.GetClassAd(q_key);
	    string buff;
	    ClassAdUnParser unparser;
	    unparser.Unparse(buff,tmp);
	    cout << "cla " << buff << endl;
    }while(a.Next(q_key)==true);
  }
  cout << " end of show " << endl;
}

/*******************************************************
 Function:
   Testing the different functions on the Collection
   switch on input
   1: Add a ClassAd
   2: Modify A ClassAd
   3: Remove A ClassAd
   4: Display A ClassAd with specified key
   6: Create a View with constrain
   7: Query ClassAd with specified constrain
   8: Make a checkpoint
   9: Truncate the storage file
   10:Truncate the log file
   11:Display all the ClassAd in the Collection
 *******************************************************/
int main(){
 string filename="serverlog";
 string storagefile="storagefile";
 string checkpointfile="checkpointfile";
 server.InitializeFromLog(filename,storagefile,checkpointfile);
 dump_collection();
 ClassAdParser parser;
 ClassAdUnParser unparser;
 ClassAd *classad_a;
 int input=1; 
 int count=1;
  while(input<12){
   cout << "input your choice" << endl;
   cin >> input;
   switch(input){
   
   case 1:{
           cout << "add a classad" << endl;
	   ClassAd* one_cla;
	   char a_cla[60];
	   
	   sprintf(a_cla, "[content=\"test\";key=%d]",count);
	   cout << a_cla << endl;
	   string buf_in(a_cla);
	   one_cla=parser.ParseClassAd(buf_in,true);
	   char ckey[10];
	   sprintf(ckey,"%d",count);
	   string key(ckey);
	   if (!server.AddClassAd( key, one_cla)) {
	           printf("AddClassAd failed!\n");
		   exit(1);
	   }
	   count++;
	   break;
   }
   case 2:{
           cout << "modify a classad" << endl;
	   char ckey[5];
	   cin.width(5);
	   cin >> ckey;
	   string key(ckey);
	   string modify_c="[ Updates  = [ content = \"modify update\"]  ]";
	   ClassAd* tmp=parser.ParseClassAd(modify_c,true);
	   if (!server.ModifyClassAd(key,tmp)){
	           printf("ModifyClassAd failed!\n");
		   exit(1);
	   };
	   break;
   }
   case 3:{
           cout << "remove a classad" << endl;
	   char ckey[5];
	   cin.width(5);
	   cin >> ckey;
	   string key(ckey);
	   if (!server.RemoveClassAd(key)){
	           printf("RemoveClassAd failed!\n");
		   exit(1);
	   }
	   break;
   }
   case 4:{
           cout << "display a classad" << endl;
	   char ckey[5];
	   cin.width(5);
	   cin >> ckey;
	   string key(ckey);
	   classad_a = server.GetClassAd(key);
	   string buf;
	   unparser.Unparse(buf,classad_a);
	   cout <<"the classad= " << buf << endl;   
	   break;
   }
   
   case 6:{
           cout << "create a view, input the name of the view" << endl;
	   string viewna;
	   cin >> viewna;
	   cout << "input the constrain" << endl;
	   string constrain;
	   cin >> constrain;
	   server.CreateSubView(viewna,"root",constrain,"","");
	   //server.CreateSubView(viewna,"root","other.key > 4 && other.key<6","","");
	   break;     
   }
   case 7:{
           ExprTree        *tree;
	   string q_key;
	   ClassAd *tmp;
	   string viewna,constr;
	   cout << "query a classad from view "<< endl;
	   cout << " input the name of the view" << endl;
	   cin >> viewna;
	   cout << "input the constrain " << endl;
	   cin >> constr;
	   
	   if( !( tree = parser.ParseExpression(constr, false) )){
	            cout << "parse error " << endl;
	   };
	   
	   LocalCollectionQuery a;
	   a.Bind(&server);//server is the collection
	   a.Query(viewna,tree);
	   a.ToFirst();
	   bool ret=a.Current(q_key);
	   
	   if (ret==true){
	     do{
	            cout << "q_key " << q_key <<endl;
		    tmp=server.GetClassAd(q_key);
		    string buff;
		    ClassAdUnParser unparser;
		    unparser.Unparse(buff,tmp);
		    cout << "cla " << buff << endl;
	     }while(a.Next(q_key)==true);
	   }else{
	     cout << "nothing " << endl;
	   }  
	   break;
   }
   case 8:{
           cout << "make a checkpoint " << endl;
	   server.WriteCheckPoint();
	   break;
   }
   case 9:{
           cout << "truncate the storage file" << endl;
	   server.TruncateStorageFile();
	   break;
   }
   case 10:{
           cout << "truncate the log file " << endl;
	   server.TruncateLog();
	   break;
   }
   case 11:{
           dump_collection();
	   server.ClassAdStorage.dump_index();
	   break;
   }     
   }
  }
}


}
