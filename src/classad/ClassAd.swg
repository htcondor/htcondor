%module ClassAd
%include "typemaps.i"
%include "std_string.i"

%{
#include "classad.h"
#include "matchClassad.h"
#include "sink.h"
#include "source.h"
#include "value.h"
#include "classadItor.h"
%}

// Perl has no "bool" so this helps.
%inline %{
	int bool_to_int( bool in ) {
		if(in)
			return 1;
		else
			return 0;
	}
%}

class ClassAdParser {
public:
	ClassAdParser();
	~ClassAdParser();
	ClassAd *ParseClassAd(char * text, bool who_knows); 
};


class Value {
public:
    Value();
    ~Value();
    enum NumberFactor {
		NO_FACTOR,  // 	No factor specified.
		B_FACTOR, 	// 	Byte factor.
		K_FACTOR, 	// 	Kilo factor.
		M_FACTOR, 	// 	Mega factor.
		G_FACTOR, 	// 	Giga factor.
		T_FACTOR, 	// 	Terra factor.
    };

    enum ValueType {
		ERROR_VALUE,
		UNDEFINED_VALUE,
		BOOLEAN_VALUE,
		INTEGER_VALUE,
		REAL_VALUE,
		RELATIVE_TIME_VALUE,
		ABSOLUTE_TIME_VALUE,
		STRING_VALUE,
		CLASSAD_VALUE,
		LIST_VALUE,
    };

    %extend {
      
        std::string Value::get_type() {
			Value val;
			val = *self;
			switch (val.GetType()) {
			case Value::UNDEFINED_VALUE:
				return "undefined";
			case Value::ERROR_VALUE:
				return "error";
			case Value::CLASSAD_VALUE:
				return "classad";
			case Value::LIST_VALUE:
				return "list";
			case Value::STRING_VALUE:
				return "string";
			case Value::BOOLEAN_VALUE:
				return "boolean";
			case Value::INTEGER_VALUE:
				return "integer";
			case Value::REAL_VALUE:
				return "real";
			case Value::ABSOLUTE_TIME_VALUE:
				return "absolute-time";
			case Value::RELATIVE_TIME_VALUE:
				return "relative-time";
			default:
				return "bad";
			}
		}
     
        int Value::is_boolean_value() {
			bool is = self->IsBooleanValue();
			return bool_to_int(is);
		}
     
        int Value::is_integer_value() {
			bool is = self->IsIntegerValue();
			return bool_to_int(is);
		}
     
        int Value::is_real_value() {
			bool is = self->IsRealValue();
			return bool_to_int(is);
		}
     
        int Value::is_string_value() {
			bool is = self->IsStringValue();
			return bool_to_int(is);
		}
	
        int Value::is_list_value() {
			bool is = self->IsListValue();
			return bool_to_int(is);
		}
     
        int Value::is_classad_value() {
			bool is = self->IsClassAdValue();
			return bool_to_int(is);
		}
     
        int Value::is_error_value() {
			bool is = self->IsErrorValue();
			return bool_to_int(is);
		}
     
        int Value::is_exceptional() {
			bool is = self->IsExceptional();
			return bool_to_int(is);
		}
     
        int Value::is_number() {
			bool is = self->IsNumber();
			return bool_to_int(is);
		}
     
        int Value::is_absolute_time_value() {
			bool is = self->IsAbsoluteTimeValue();
			return bool_to_int(is);
		}
     
        int Value::is_relative_time_value() {
			bool is = self->IsRelativeTimeValue();
			return bool_to_int(is);
		}
     
        int Value::get_boolean_value() {
			bool is, val;
			is = self->IsBooleanValue(val);
			if (is) {
				return bool_to_int(val);
			} else {
				return NULL;
			}
		}
     
        int Value::get_integer_value() {
			int rv;
			bool is = self->IsIntegerValue(rv);
			return is ? rv : NULL;
		}
     
        double Value::get_real_value() {
			double rv;
			bool is = self->IsRealValue(rv);
			return is ? rv : NULL;
		}
     
        std::string Value::get_string_value() {
			std::string rv;
			bool is = self->IsStringValue(rv);
			return is ? rv : NULL;
		}

//        int Value::get_list_value() {}
	
        ClassAd* Value::get_classad_value() {
			ClassAd* rv;
			bool is = self->IsClassAdValue(rv);
			return is ? rv : NULL;
		}
  
/*   
        int Value::get_error_value() {}
     
        int Value::get_exceptional() {}
     
        int Value::get_absolute_time_value() {}
     
        int Value::get_relative_time_value() {}
	*/

        int Value::same_as(Value otherValue) {
			bool same = self->SameAs(otherValue);
			return bool_to_int(same);
		}
    }
};

class ClassAdIterator {
public:
	ClassAdIterator();
	~ClassAdIterator();

	%extend {

		ClassAdIterator( ClassAd* ca ) {
			ClassAdIterator *it = new ClassAdIterator(*ca);

			return it;
		}

		void ClassAdIterator::to_first() {
			self->ToFirst();
		}

		void ClassAdIterator::to_after_last() {
			self->ToAfterLast();
		}

		std::string ClassAdIterator::next_attribute() {
			std::string attr;
			ExprTree* expr;
			bool iserr = self->NextAttribute(attr, (const ExprTree*&)expr);

			return attr;
		}

		std::string ClassAdIterator::current_attribute() {
			std::string attr;
			ExprTree* expr;
			self->CurrentAttribute(attr, (const ExprTree*&)expr);

			return attr;
		}

		int ClassAdIterator::is_at_first() {
			bool isfirst = self->IsAtFirst();
			return bool_to_int(isfirst);
		}

		int ClassAdIterator::is_after_last() {
			bool islast = self->IsAfterLast();
			return bool_to_int(islast);
		}

	}
};

class ClassAd {
public:
    ClassAd();
    ~ClassAd();
    
    %extend {
      
			/* Wraps the classad method "ParseClassAd"
			   Creates classad object corresponding to its string 
			   representation */
		ClassAd( std::string classad_text ) {
            ClassAdParser parser;
            ClassAd* rv = parser.ParseClassAd(classad_text, true);
            return rv;
        }

		int ClassAd::insert( std::string attrName, ExprTree* expr ) {
			bool iserr = self->Insert(attrName, expr);
			return bool_to_int(iserr);
		}

		int ClassAd::deep_insert( ExprTree *scopeExpr, std::string attrName, ExprTree *expr ) {
			bool iserr = self->DeepInsert(scopeExpr, attrName, expr);
			return bool_to_int(iserr);
		}

		int ClassAd::insert_attr_classad( std::string attrName, ClassAd* value ) {
			bool iserr = self->Insert(attrName, (ExprTree *)value);
			return bool_to_int(iserr);
		}

		int ClassAd::deep_insert_attr_classad( ExprTree *scopeExpr, std::string attrName, ClassAd* value ) {
			bool iserr = self->DeepInsert(scopeExpr, attrName, (ExprTree *)value);
			return bool_to_int(iserr);
		}

		int ClassAd::insert_attr_int( std::string attrName, int value ) {
			bool iserr = self->InsertAttr(attrName, value);
			return bool_to_int(iserr);
		}

		int ClassAd::deep_insert_attr_int( ExprTree *scopeExpr, std::string attrName, int value ) {
			bool iserr = self->DeepInsertAttr(scopeExpr, attrName, value);
			return bool_to_int(iserr);
		}

		int ClassAd::insert_attr_double( std::string attrName, double value ) {
			bool iserr = self->InsertAttr(attrName, value);
			return bool_to_int(iserr);
		}

		int ClassAd::deep_insert_attr_double( ExprTree *scopeExpr, std::string attrName, double value ) {
			bool iserr = self->DeepInsertAttr(scopeExpr, attrName, value);
			return bool_to_int(iserr);
		}

		int ClassAd::insert_attr_bool( std::string attrName, int value ) {
			bool iserr = self->InsertAttr(attrName, value);
			return bool_to_int(iserr);
		}

		int ClassAd::deep_insert_attr_bool( ExprTree *scopeExpr, std::string attrName, int value ) {
			bool iserr = self->DeepInsertAttr(scopeExpr, attrName, value);
			return bool_to_int(iserr);
		}

		int ClassAd::insert_attr_string( std::string attrName, 
										 std::string value ) {
			bool iserr = self->InsertAttr(attrName, value);
			return bool_to_int(iserr);
		}

		int ClassAd::deep_insert_attr_string( ExprTree *scopeExpr, std::string attrName, std::string value ) {
			bool iserr = self->DeepInsertAttr(scopeExpr, attrName, value);
			return bool_to_int(iserr);
		}

		ExprTree* ClassAd::lookup( std::string attrName ) {
			ExprTree *rv = self->Lookup(attrName);
			return rv;
		}

// ExprTree* ClassAd::lookup_in_scope( std::string attrName, ClassAd *ad ) {}

		void ClassAd::clear() {
			self->Clear();
		}

		int ClassAd::delete_attr( std::string attrName ) {
			bool iserr = self->Delete(attrName);
			return bool_to_int(iserr);
		}

		int ClassAd::deep_delete( ExprTree *scopeExpr, std::string attrName ) {
			bool iserr = self->DeepDelete(scopeExpr, attrName);
			return bool_to_int(iserr);
		}

		ExprTree* ClassAd::remove( std::string attrName ) {
			ExprTree* rv = self->Remove(attrName);
			return rv;
		}

//	ExprTree* ClassAd::deep_remove( ExprTree *scopeExpr, std::string attrName ) {}
	
		Value ClassAd::evaluate_attr( std::string attrName, int *OUTPUT ) {
			Value val;
			bool iserr = self->EvaluateAttr(attrName, val);
			*OUTPUT = bool_to_int(iserr);
			return val;
		}

		Value ClassAd::evaluate_expr_expr( ExprTree *expr, int *OUTPUT ) {
			Value val;
			bool iserr = self->EvaluateExpr(expr, val);
			*OUTPUT = bool_to_int(iserr);
			return val;
		}

		Value ClassAd::evaluate_expr_string( std::string expr, int *OUTPUT ) {
			Value val;
			bool iserr = self->EvaluateExpr(expr, val);
			*OUTPUT = bool_to_int(iserr);
			return val;
		}

		int ClassAd::evaluate_attr_int( std::string attrName, int *OUTPUT ) {
            int rv;
            bool iserr = self->EvaluateAttrInt(attrName, rv);
            *OUTPUT = bool_to_int(iserr);
            return rv;
		}

		double ClassAd::evaluate_attr_double( std::string attrName, 
											  double *OUTPUT ) {
            double rv;
            bool iserr = self->EvaluateAttrNumber(attrName, rv);
            *OUTPUT = bool_to_int(iserr);
            return rv;
		}

		std::string ClassAd::evaluate_attr_string( std::string attrName, 
												   int *OUTPUT ) {
            std::string rv;
            bool iserr = self->EvaluateAttrString(attrName, rv);
            *OUTPUT = bool_to_int(iserr);
            return rv;
		}

		int ClassAd::evaluate_attr_bool( std::string attrName, int *OUTPUT ) {
            bool iserr, rv;
            iserr = self->EvaluateAttrBool(attrName, rv);
            *OUTPUT = bool_to_int(iserr);
            return bool_to_int(rv);
		}

		void ClassAd::update( ClassAd ad ) {
			self->Update(ad);
		}

		void ClassAd::modify( ClassAd ad ) {
			self->Modify(ad);
		}

		ClassAd* ClassAd::copy() {
			ExprTree* tree = self->Copy();
			ClassAd* rv = (ClassAd *)tree;
			return rv;
		}

		int ClassAd::copy_from( ClassAd ad ) {
			bool iserr = self->CopyFrom(ad);
			return bool_to_int(iserr);
		}

		int ClassAd::same_as( ClassAd ad ) {
			ExprTree* tree = (ExprTree *)&ad;
			bool same = self->SameAs(tree);
			return bool_to_int(same);
		}


		Value ClassAd::flatten( ClassAd* ad ) {
			Value val;
			ExprTree* fexpr;
			ExprTree* expr = (ExprTree *)ad;
			self->Flatten(expr, val, fexpr);

			if (fexpr != NULL) {
				val = Value();
				val.SetClassAdValue((ClassAd *)fexpr);
			}
			
			return val;
		}

//	int ClassAd::get_external_references( ExprTree *tree, PortReferences refs ) {}

        std::string ClassAd::unparse() {
            PrettyPrint printer;
            std::string printed_classad;
            printer.Unparse( printed_classad, self );
            return printed_classad;
        }
    } 
};

class ExprTree : public ClassAd {
public:
	%extend {
		ExprTree( std::string expr_text ) {
			ClassAdParser parser;
			ExprTree *rv = parser.ParseExpression(expr_text, true);
			return rv;
		}
	}
};

class MatchClassAd : public ClassAd {
public:
    MatchClassAd();
    MatchClassAd( ClassAd* al, ClassAd* ar );
    ~MatchClassAd();
};
