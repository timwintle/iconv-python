#include <iconv.h>
#include <Python.h>

typedef struct {
	PyObject_HEAD
	iconv_t handle;
} IconvObject;

static PyObject *error;

staticforward PyTypeObject Iconv_Type;

static char iconv_open__doc__[]=
"open(tocode, fromcode) -> iconv handle\n"
"allocate descriptor for character set conversion";

static PyObject*
py_iconv_open(PyObject* unused, PyObject* args)
{
	char *tocode, *fromcode;
	iconv_t result;
	IconvObject *self;
	if (!PyArg_ParseTuple(args, "ss", &tocode, &fromcode))
		return NULL;
	result = iconv_open(tocode, fromcode);
	if (result == (iconv_t)(-1)){
		PyErr_SetFromErrno(PyExc_ValueError);
		return NULL;
	}
	self = PyObject_New(IconvObject, &Iconv_Type);
	if (self == NULL){
		iconv_close(result);
		return NULL;
	}
	self->handle = result;
	return (PyObject*)self;
}

static void
Iconv_dealloc(IconvObject *self)
{
	iconv_close(self->handle);
	PyObject_Del(self);
}

static inline int python_unicode_width(void) {
    return PyUnicode_GetMax() == 1114111 ? 4 : 2;
}

static char Iconv_iconv__doc__[]=
"iconv(in[, outlen[, return_unicode[, count_only]]]) -> out\n"
"Convert in to out. outlen is the size of the output buffer;\n"
"it defaults to len(in).";

static PyObject*
Iconv_iconv(IconvObject *self, PyObject *args, PyObject* kwargs)
{
    int unicode_width = python_unicode_width();
	PyObject *inbuf_obj;
	const char *inbuf;
	char *outbuf;
	size_t inbuf_size, outbuf_size, iresult;
	int inbuf_size_int, outbuf_size_int = -1;
	int return_unicode = 0, count_only = 0;
	PyObject *result;
	static char *kwarg_names[]={
		"s",
		"outlen",
		"return_unicode",
		"count_only",
		NULL
	};
	if (!PyArg_ParseTupleAndKeywords(args, kwargs, 
					 "O|iii:iconv", kwarg_names,
					 &inbuf_obj, &outbuf_size_int,
					 &return_unicode, &count_only))
		return NULL;

	if (inbuf_obj == Py_None){
		/* None means to clear the iconv object */
		inbuf = NULL;
		inbuf_size_int = 0;
	}else if (inbuf_obj->ob_type->tp_as_buffer){
		if (PyObject_AsReadBuffer(inbuf_obj, (const void**)&inbuf, 
					  &inbuf_size_int) == -1)
			return NULL;
	}else{
		PyErr_SetString(PyExc_TypeError, 
				"iconv expects string as first argument");
		return NULL;
	}
	/* If no result size estimate was given, estimate that the result
	   string is the same size as the input string. */
	if (outbuf_size_int == -1)
		outbuf_size_int = inbuf_size_int;
	inbuf_size = inbuf_size_int;
	if (count_only){
		result = NULL;
		outbuf = NULL;
		outbuf_size = outbuf_size_int;
	}else if(return_unicode){
		/* Allocate the result string. */
		outbuf_size = outbuf_size_int*unicode_width;
		outbuf = malloc(outbuf_size);
	}else{
		/* Allocate the result string. */
		outbuf_size = outbuf_size_int;
		outbuf = malloc(outbuf_size);
	}
	
	char *iconv_outbuf = outbuf;
	/* Perform the conversion. */
	iresult = iconv(self->handle, &inbuf, &inbuf_size, &iconv_outbuf, &outbuf_size);

	if (iresult == -1){
		PyObject *exc;
		exc = PyObject_CallFunction(error,"siiO",
					    strerror(errno),errno,
					    inbuf_size_int - inbuf_size,
					    Py_None);
		if (outbuf != NULL) {
		    free(outbuf);
		}
		PyErr_SetObject(error,exc);
		return NULL;
	}
	
	if (count_only){
		result = PyInt_FromLong(outbuf_size_int-outbuf_size);
	}else if (return_unicode) {
		/* If the conversion was successful, the result string may be
		   larger than necessary; outbuf_size will present the extra
		   bytes. */
		result = PyUnicode_FromUnicode(outbuf, 
		        outbuf_size_int-(outbuf_size/unicode_width));
		free(outbuf);
	}else{
		result = PyString_FromStringAndSize(outbuf, outbuf_size_int - outbuf_size);
		free(outbuf);
	}
	
	return result;
}

static char Iconv_set_initial__doc__[]=
"set_initial([outlen]) -> out\n"
"Reset codec to initial state. If outlen is non-zero, it attempts\n"
"to return up to outlen bytes to emit the proper shift sequence.";

static PyObject*
Iconv_set_initial(IconvObject *self, PyObject *args)
{
	int outbuf_size_int = 0;
	char *outbuf;
	size_t outbuf_size, iresult;
	PyObject *result;
	if (!PyArg_ParseTuple(args, "|i:set_initial", 
			      &outbuf_size_int))
		return NULL;
	if (outbuf_size_int == 0) {
		iresult = iconv(self->handle, NULL, 0, NULL, 0);
		if (iresult != 0) {
			PyErr_SetString(PyExc_RuntimeError,
					"Resetting codec failed");
			return NULL;
		}
		return PyString_FromString("");
	}
	outbuf = malloc(outbuf_size_int);
	char *iconv_outbuf = outbuf;
	outbuf_size = outbuf_size_int;
	iresult = iconv(self->handle, NULL, 0, &iconv_outbuf, &outbuf_size);

	result = PyString_FromStringAndSize(iconv_outbuf,
	                outbuf_size_int - outbuf_size);
	
	free(iconv_outbuf);
	
	if (!result)
		return NULL;

	if (iresult == -1){
		PyObject *exc;
		exc = PyObject_CallFunction(error,"siiO",
					    strerror(errno),errno,
					    0, 
					    result);
		Py_DECREF(result);
		PyErr_SetObject(error,exc);
		return NULL;
	}
	return result;
}

static PyMethodDef Iconv_methods[] = {
	{"iconv",	(PyCFunction)Iconv_iconv,	
	 METH_KEYWORDS|METH_VARARGS,	Iconv_iconv__doc__},
	{"set_initial",	(PyCFunction)Iconv_set_initial,	
	 METH_VARARGS,	Iconv_set_initial__doc__},
	{NULL,		NULL}		/* sentinel */
};

static PyObject *
Iconv_getattr(PyObject *self, char *name)
{
	return Py_FindMethod(Iconv_methods, self, name);
}

statichere PyTypeObject Iconv_Type = {
	PyObject_HEAD_INIT(NULL)
	0,			/*ob_size*/
	"Iconv",		/*tp_name*/
	sizeof(IconvObject),	/*tp_basicsize*/
	0,			/*tp_itemsize*/
	/* methods */
	(destructor)Iconv_dealloc, /*tp_dealloc*/
	0,			/*tp_print*/
	(getattrfunc)Iconv_getattr, /*tp_getattr*/
	0,			/*tp_setattr*/
	0,			/*tp_compare*/
	0,			/*tp_repr*/
	0,			/*tp_as_number*/
	0,			/*tp_as_sequence*/
	0,			/*tp_as_mapping*/
	0,			/*tp_hash*/
};

static PyMethodDef iconv_methods[] = {
	{"open",	py_iconv_open,
	 METH_VARARGS,	iconv_open__doc__},
	{NULL,		NULL}		/* sentinel */
};

static char __doc__[]=
"The iconv module provides an interface to the iconv library.";

DL_EXPORT(void)
initiconv(void)
{
	PyObject *m, *d;

	Iconv_Type.ob_type = &PyType_Type;

	/* Create the module and add the functions */
	m = Py_InitModule4("iconv", iconv_methods, __doc__, 
			   NULL, PYTHON_API_VERSION);

	/* Add some symbolic constants to the module */
	d = PyModule_GetDict(m);
	error = PyErr_NewException("iconv.error", PyExc_ValueError, NULL);
	PyDict_SetItemString(d, "error", error);
}
