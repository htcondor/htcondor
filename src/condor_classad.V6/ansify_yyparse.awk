/^yyparse/	{	
				print "yyparse (void *YYPARSE_PARAM)";
				getline;
				getline;
			}	
			{
				print $0;
			}
