/*  $Id$

node.js addon for SWI-Prolog

Author:        Tom Klonikowski
E-mail:        klonik_t@informatik.haw-hamburg.de
Copyright (C): 2012, Tom Klonikowski

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <v8.h>
#include <node.h>
#include <string.h>
#include <stdlib.h>

#include <SWI-Prolog.h>
#include "libswipl.h"

using namespace v8;
using namespace node;

#define LOG 1

#ifdef LOG
#define QLOG2(f, a1) if (obj->cb_log) obj->cb_log((f), (a1))
#else
#define QLOG2(_1, _2) while (false) 
#endif

Handle<Object> ExportSolution(term_t t, int len, Handle<Object> result_terms,
							  Handle<Object> varnames);

module_t GetModule(const Arguments& args, int idx) {
	module_t mo = NULL;
	if (args.Length() > idx && !(args[idx]->IsUndefined() || args[idx]->IsNull())
		&& args[idx]->IsString()) {
			mo = PL_new_module(PL_new_atom(*String::Utf8Value(args[idx])));
	}
	return mo;
}

Handle<Value> Initialise(const Arguments& args) {
	int rval;
	const char *plav[2];
	HandleScope scope;

	/* make the argument vector for Prolog */

	String::Utf8Value str(args[0]);
	plav[0] = *str;
	plav[1] = "--quiet";
	plav[2] = NULL;

	/* initialise Prolog */

	rval = PL_initialise(2, (char **) plav);

	return scope.Close(Number::New(rval));
}

Handle<Value> CreateException(const char *msg) {
	Handle<Object> result = Object::New();
	result->Set(String::New("exc"), String::New(msg));
	return result;
}

const char* GetExceptionString(term_t term) {
	char *msg;
	term_t msgterms = PL_new_term_refs(2);
	PL_put_term(msgterms, term);
	int rval = PL_call_predicate(NULL, PL_Q_NODEBUG,
		PL_predicate("message_to_string", 2, NULL), msgterms);
	if (rval) {
		rval = PL_get_chars(msgterms + 1, &msg, CVT_ALL);
		return msg;
	} else {
		return "unknown error";
	}
}

/**
*
*/
Handle<Value> TermType(const Arguments& args) {
	int rval = 0;
	HandleScope scope;
	term_t term = PL_new_term_ref();
	rval = PL_chars_to_term(*String::Utf8Value(args[0]), term);
	if (rval) {
		rval = PL_term_type(term);
	}
	return scope.Close(Number::New(rval));
}

void ExportTerm(term_t t, Handle<Object> result, Handle<Object> varnames) {
	int rval = 0;
	Handle<Value> val;
	char *c = NULL;
	Handle<Value> key;

	{
		char ptrtostr[8];
		int n = sprintf(ptrtostr, "%0*"PRIxPTR, PRIxPTR_WIDTH, (intptr_t)t);
		key = varnames->Get(String::New(ptrtostr, n));
	}

	int i = 0;
	double d = 0.0;
	int type = PL_term_type(t);
	if (!key->IsUndefined()) {
		switch (type) {
		case PL_FLOAT:
			rval = PL_get_float(t, &d);
			val = Number::New(d);
			break;
		case PL_INTEGER:
			rval = PL_get_integer(t, &i);
			val = Integer::New(i);
			break;
		case PL_LIST:
			// TODO: rval
			val = Array::New(0);
			break;
		default:
			rval = PL_get_chars(t, &c, CVT_ALL);
			val = String::New(c);
			break;
		}
		if (rval) {
			result->Set(key, val);
		}
	}
}

Handle<Object> ExportSolution(term_t t, int len, Handle<Object> result_terms,
							  Handle<Object> varnames) {
								  for (int j = 0; j < len; j++) {
									  ExportTerm(t + j, result_terms, varnames);
								  }
								  return result_terms;
}

Handle<Value> Cleanup(const Arguments& args) {
	HandleScope scope;
	int rval = PL_cleanup(0);
	return scope.Close(Number::New(rval));
}

Query::Query() {
}

Query::~Query() {
}

void Query::Init(Handle<Object> target) {
	// Prepare constructor template
	Local<FunctionTemplate> tpl = FunctionTemplate::New(Open);
	tpl->SetClassName(String::NewSymbol("Query"));
	tpl->InstanceTemplate()->SetInternalFieldCount(3);
	// Prototype
	tpl->PrototypeTemplate()->Set(String::NewSymbol("next_solution"),
		FunctionTemplate::New(NextSolution)->GetFunction());
	tpl->PrototypeTemplate()->Set(String::NewSymbol("close"),
		FunctionTemplate::New(Close)->GetFunction());
	tpl->PrototypeTemplate()->Set(String::NewSymbol("exception"),
		FunctionTemplate::New(Exception)->GetFunction());

	Persistent<Function> constructor = Persistent<Function>::New(
		tpl->GetFunction());
	target->Set(String::NewSymbol("Query"), constructor);
}

Handle<Value> Query::Open(const Arguments& args) {
	HandleScope scope;

	Query* obj = new Query();
#ifdef LOG
	obj->cb_log = &printf;
#else
	obj->cb_log = NULL;
#endif

	module_t module = GetModule(args, 2);
	const char *module_name =
		module ? PL_atom_chars(PL_module_name(module)) : NULL;

	if (args.Length() > 1 && args[0]->IsString() && args[1]->IsArray()) {
		int rval = 0;
		String::Utf8Value predicate(args[0]);
		QLOG2("Query::Open predicate: %s(", *predicate);
		Handle<Array> terms = Handle<Array>::Cast(args[1]);
		predicate_t p = PL_predicate(*predicate, terms->Length(), module_name);
		obj->term = PL_new_term_refs(terms->Length());
		obj->term_len = terms->Length();
		obj->varnames = Persistent<Object>::New(Object::New());
		term_t t = obj->term;
		unsigned int numberOfTerms = terms->Length();
		for (unsigned int i = 0; i < numberOfTerms; i++) {
			Local<Value> v = terms->Get(i);

			if (v->IsInt32()) {
				QLOG2("%i : int", v->Int32Value());
				rval = PL_put_integer(t, v->Int32Value());
			} else if (v->IsNumber()) {
				QLOG2("%f : double", v->NumberValue());
				rval = PL_put_float(t, v->NumberValue());
			} else if (v->IsArray()) {
				QLOG2("%s", "[] : list");
				PL_put_nil(t);
				rval = true;
			} else {
				String::Utf8Value s(v);
				QLOG2("%s", *s);
				rval = PL_chars_to_term(*s, t);
				int type = PL_term_type(t);
				switch (type) {
				case PL_VARIABLE:
					{
						char ptrtostr[8];
						int n = sprintf(ptrtostr, "%0*"PRIxPTR, PRIxPTR_WIDTH,  (intptr_t)t);
						obj->varnames->Set(String::New(ptrtostr, n), String::New(*s));
					}
					QLOG2(" : %s", "var");
					break;
#ifdef LOG
				case PL_ATOM:
					QLOG2(" : %s", "atom");
					break;
				case PL_TERM:
					QLOG2(" : %s", "term");
					break;
				default:
					QLOG2(" : type#%d", type);
					break;
				}
#endif
			}
			QLOG2("%s", i < numberOfTerms-1 ? ", " : "");
			t = t + 1;
		}

		obj->qid = PL_open_query(module, PL_Q_CATCH_EXCEPTION, p, obj->term);

		QLOG2(") #%li\n", obj->qid);

		if (obj->qid == 0) {
			ThrowException(
				Exception::Error(
				String::New("not enough space on the environment stack")));
			return scope.Close(Undefined());
		} else if (rval == 0) {
			ThrowException(
				Exception::Error(
				String::New(GetExceptionString(PL_exception(obj->qid)))));
			return scope.Close(Undefined());
		} else {
			obj->open = OPEN;
			obj->Wrap(args.This());
			return args.This();
		}
	} else {
		ThrowException(
			Exception::SyntaxError(
			String::New("invalid arguments (pred, [ args ], module)")));
		return scope.Close(Undefined());
	}
}

Handle<Value> Query::NextSolution(const Arguments& args) {
	HandleScope scope;
	int rval = 0;

	Query* obj = ObjectWrap::Unwrap<Query>(args.This());

	QLOG2("Query::NextSolution #%li", obj->qid);

	if (obj->open == OPEN) {
		rval = PL_next_solution(obj->qid);
		QLOG2(": %i\n", rval);

		if (rval) {
			return scope.Close(
				ExportSolution(obj->term, obj->term_len, Object::New(),
				obj->varnames));
		} else {
			return scope.Close(Boolean::New(false));
		}
	} else {
		ThrowException(Exception::Error(String::New("query is closed")));
		return scope.Close(Undefined());
	}
}

Handle<Value> Query::Exception(const Arguments& args) {
	HandleScope scope;

	Query* obj = ObjectWrap::Unwrap<Query>(args.This());
	QLOG2("Query::Exception #%li\n", obj->qid);
	term_t term = PL_exception(obj->qid);

	if (term) {
		return scope.Close(CreateException(GetExceptionString(term)));
	} else {
		return scope.Close(Boolean::New(false));
	}
}

Handle<Value> Query::Close(const Arguments& args) {
	HandleScope scope;

	Query* obj = ObjectWrap::Unwrap<Query>(args.This());
	if (obj->open == OPEN) {
		QLOG2("Query::Close #%li\n", obj->qid);
		PL_close_query(obj->qid);
		obj->open = CLOSED;
	}

	return scope.Close(Boolean::New(true));
}

extern "C" void init(Handle<Object> target) {
	target->Set(String::NewSymbol("initialise"),
		FunctionTemplate::New(Initialise)->GetFunction());
	target->Set(String::NewSymbol("term_type"),
		FunctionTemplate::New(TermType)->GetFunction());
	target->Set(String::NewSymbol("cleanup"),
		FunctionTemplate::New(Cleanup)->GetFunction());
	Query::Init(target);
}

NODE_MODULE(libswipl, init)

