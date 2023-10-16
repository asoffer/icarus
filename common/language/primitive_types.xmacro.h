#if not defined(IC_XMACRO_PRIMITIVE_TYPE)
#error `IC_XMACRO_PRIMITIVE_TYPE` must be defined.
#endif

// Defines an X-macro enabling iteration over all primitive types in the
// language. Three arguments are provided to each macro invocation.
//
// Argument 1: The token representing the enumerator in
//             `type::PrimitiveType::Kind`.
// Argument 2: The global symbol in the `type` namespace used to reference this
//             type. Note that these two match identically with the exception of
//             `Type` vs. `Type_`, so as to avoid collision with the struct
//             `type::Type`.
// Argument 3: The spelling of the given type in the language.

IC_XMACRO_PRIMITIVE_TYPE(Error, Error, "error")
IC_XMACRO_PRIMITIVE_TYPE(Bool, Bool, "bool")
IC_XMACRO_PRIMITIVE_TYPE(Char, Char, "char")
IC_XMACRO_PRIMITIVE_TYPE(Byte, Byte, "byte")
IC_XMACRO_PRIMITIVE_TYPE(I8, I8, "i8")
IC_XMACRO_PRIMITIVE_TYPE(I16, I16, "i16")
IC_XMACRO_PRIMITIVE_TYPE(I32, I32, "i32")
IC_XMACRO_PRIMITIVE_TYPE(I64, I64, "i64")
IC_XMACRO_PRIMITIVE_TYPE(U8, U8, "u8")
IC_XMACRO_PRIMITIVE_TYPE(U16, U16, "u16")
IC_XMACRO_PRIMITIVE_TYPE(U32, U32, "u32")
IC_XMACRO_PRIMITIVE_TYPE(U64, U64, "u64")
IC_XMACRO_PRIMITIVE_TYPE(F32, F32, "f32")
IC_XMACRO_PRIMITIVE_TYPE(F64, F64, "u64")
IC_XMACRO_PRIMITIVE_TYPE(Integer, Integer, "integer")
IC_XMACRO_PRIMITIVE_TYPE(Type, Type_, "type")
IC_XMACRO_PRIMITIVE_TYPE(Module, Module, "module")

#undef IC_XMACRO_PRIMITIVE_TYPE