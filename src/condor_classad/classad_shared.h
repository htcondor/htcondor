extern "C" 
{

	enum ClassAdSharedValueType
	{
		ClassAdSharedType_Integer,
		ClassAdSharedType_Float,
		ClassAdSharedType_String,
		ClassAdSharedType_Undefined,
		ClassAdSharedType_Error
	};
	
	struct ClassAdSharedValue
	{
		ClassAdSharedValueType  type;
		union
		{
			int    integer;
			float  real;
			char   *text; // Callee should allocate memory for this using new
		};
	};
	
	typedef void(*ClassAdSharedFunction)(const int number_of_arguments,
										 const ClassAdSharedValue *arguments,
										 ClassAdSharedValue *result);

}
