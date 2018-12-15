OP_MACRO(Death,                Death,               void,                    empty_)
OP_MACRO(Bytes,                Bytes,               type::Type const*,       type_arg_)
OP_MACRO(Align,                Align,               type::Type const*,       type_arg_)
OP_MACRO(NotBool,              Not,                 bool,                    reg_)
OP_MACRO(NotFlags,             Not,                FlagsVal,                 typed_reg_)
OP_MACRO(NegInt8,              Neg,                 i8,                      reg_)
OP_MACRO(NegInt16,             Neg,                 i16,                     reg_)
OP_MACRO(NegInt32,             Neg,                 i32,                     reg_)
OP_MACRO(NegInt64,             Neg,                 i64,                     reg_)
OP_MACRO(NegFloat32,           Neg,                 float,                   reg_)
OP_MACRO(NegFloat64,           Neg,                 double,                  reg_)
OP_MACRO(LoadBool,             Load,                bool,                    reg_)
OP_MACRO(LoadInt8,             Load,                 i8,                     reg_)
OP_MACRO(LoadInt16,            Load,                i16,                     reg_)
OP_MACRO(LoadInt32,            Load,                i32,                     reg_)
OP_MACRO(LoadInt64,            Load,                i64,                     reg_)
OP_MACRO(LoadNat8,             Load,                 u8,                     reg_)
OP_MACRO(LoadNat16,            Load,                u16,                     reg_)
OP_MACRO(LoadNat32,            Load,                u32,                     reg_)
OP_MACRO(LoadNat64,            Load,                u64,                     reg_)
OP_MACRO(LoadFloat32,          Load,                float,                   reg_)
OP_MACRO(LoadFloat64,          Load,                double,                  reg_)
OP_MACRO(LoadType,             Load,                type::Type const*,       reg_)
OP_MACRO(LoadEnum,             Load,                EnumVal,                 reg_)
OP_MACRO(LoadFlags,            Load,                FlagsVal,                reg_)
OP_MACRO(LoadAddr,             Load,                ir::Addr,                reg_)
OP_MACRO(LoadFunc,             Load,                AnyFunc,                 reg_)
OP_MACRO(StoreBool,            Store,               bool,                    store_bool_)
OP_MACRO(StoreInt8,            Store,               i8,                      store_i8_)
OP_MACRO(StoreInt16,           Store,               i16,                     store_i16_)
OP_MACRO(StoreInt32,           Store,               i32,                     store_i32_)
OP_MACRO(StoreInt64,           Store,               i64,                     store_i64_)
OP_MACRO(StoreNat8,            Store,               u8,                      store_u8_)
OP_MACRO(StoreNat16,           Store,               u16,                     store_u16_)
OP_MACRO(StoreNat32,           Store,               u32,                     store_u32_)
OP_MACRO(StoreNat64,           Store,               u64,                     store_u64_)
OP_MACRO(StoreFloat32,         Store,               float,                   store_float32_)
OP_MACRO(StoreFloat64,         Store,               double,                  store_float64_)
OP_MACRO(StoreType,            Store,               type::Type const*,       store_type_)
OP_MACRO(StoreEnum,            Store,               EnumVal,                 store_enum_)
OP_MACRO(StoreFunc,            Store,               ir::AnyFunc,             store_func_)
OP_MACRO(StoreFlags,           Store,               FlagsVal,                store_flags_)
OP_MACRO(StoreAddr,            Store,               ir::Addr,                store_addr_)
OP_MACRO(PrintBool,            Print,               bool,                    bool_arg_)
OP_MACRO(PrintInt8,            Print,               i8,                      i8_arg_)
OP_MACRO(PrintInt16,           Print,               i16,                     i16_arg_)
OP_MACRO(PrintInt32,           Print,               i32,                     i32_arg_)
OP_MACRO(PrintInt64,           Print,               i64,                     i64_arg_)
OP_MACRO(PrintNat8,            Print,               u8,                      u8_arg_)
OP_MACRO(PrintNat16,           Print,               u16,                     u16_arg_)
OP_MACRO(PrintNat32,           Print,               u32,                     u32_arg_)
OP_MACRO(PrintNat64,           Print,               u64,                     u64_arg_)
OP_MACRO(PrintFloat32,         Print,               float,                   float32_arg_)
OP_MACRO(PrintFloat64,         Print,               double,                  float64_arg_)
OP_MACRO(PrintType,            Print,               type::Type const*,       type_arg_)
OP_MACRO(PrintEnum,            Print,               EnumVal,                 print_enum_)
OP_MACRO(PrintFlags,           Print,               FlagsVal,                print_flags_)
OP_MACRO(PrintAddr,            Print,               ir::Addr,                addr_arg_)
OP_MACRO(PrintCharBuffer,      Print,               std::string_view,        char_buf_arg_)
OP_MACRO(AddInt8,              Add,                 i8,                      i8_args_)
OP_MACRO(AddInt16,             Add,                i16,                      i16_args_)
OP_MACRO(AddInt32,             Add,                i32,                      i32_args_)
OP_MACRO(AddInt64,             Add,                i64,                      i64_args_)
OP_MACRO(AddNat8,              Add,                 u8,                      u8_args_)
OP_MACRO(AddNat16,             Add,                u16,                      u16_args_)
OP_MACRO(AddNat32,             Add,                u32,                      u32_args_)
OP_MACRO(AddNat64,             Add,                u64,                      u64_args_)
OP_MACRO(AddFloat32,           Add,                 float,                   float32_args_)
OP_MACRO(AddFloat64,           Add,                 double,                  float64_args_)
OP_MACRO(SubInt8,              Sub,                 i8,                      i8_args_)
OP_MACRO(SubInt16,             Sub,                i16,                      i16_args_)
OP_MACRO(SubInt32,             Sub,                i32,                      i32_args_)
OP_MACRO(SubInt64,             Sub,                i64,                      i64_args_)
OP_MACRO(SubNat8,              Sub,                 u8,                      u8_args_)
OP_MACRO(SubNat16,             Sub,                u16,                      u16_args_)
OP_MACRO(SubNat32,             Sub,                u32,                      u32_args_)
OP_MACRO(SubNat64,             Sub,                u64,                      u64_args_)
OP_MACRO(SubFloat32,           Sub,                 float,                   float32_args_)
OP_MACRO(SubFloat64,           Sub,                 double,                  float64_args_)
OP_MACRO(MulInt8,              Mul,                 i8,                      i8_args_)
OP_MACRO(MulInt16,             Mul,                i16,                      i16_args_)
OP_MACRO(MulInt32,             Mul,                i32,                      i32_args_)
OP_MACRO(MulInt64,             Mul,                i64,                      i64_args_)
OP_MACRO(MulNat8,              Mul,                 u8,                      u8_args_)
OP_MACRO(MulNat16,             Mul,                u16,                      u16_args_)
OP_MACRO(MulNat32,             Mul,                u32,                      u32_args_)
OP_MACRO(MulNat64,             Mul,                u64,                      u64_args_)
OP_MACRO(MulFloat32,           Mul,                 float,                   float32_args_)
OP_MACRO(MulFloat64,           Mul,                 double,                  float64_args_)
OP_MACRO(DivInt8,              Div,                 i8,                      i8_args_)
OP_MACRO(DivInt16,             Div,                i16,                      i16_args_)
OP_MACRO(DivInt32,             Div,                i32,                      i32_args_)
OP_MACRO(DivInt64,             Div,                i64,                      i64_args_)
OP_MACRO(DivNat8,              Div,                 u8,                      u8_args_)
OP_MACRO(DivNat16,             Div,                u16,                      u16_args_)
OP_MACRO(DivNat32,             Div,                u32,                      u32_args_)
OP_MACRO(DivNat64,             Div,                u64,                      u64_args_)
OP_MACRO(DivFloat32,           Div,                 float,                   float32_args_)
OP_MACRO(DivFloat64,           Div,                 double,                  float64_args_)
OP_MACRO(ModInt8,              Mod,                 i8,                      i8_args_)
OP_MACRO(ModInt16,             Mod,                i16,                      i16_args_)
OP_MACRO(ModInt32,             Mod,                i32,                      i32_args_)
OP_MACRO(ModInt64,             Mod,                i64,                      i64_args_)
OP_MACRO(ModNat8,              Mod,                 u8,                      u8_args_)
OP_MACRO(ModNat16,             Mod,                u16,                      u16_args_)
OP_MACRO(ModNat32,             Mod,                u32,                      u32_args_)
OP_MACRO(ModNat64,             Mod,                u64,                      u64_args_)
OP_MACRO(LtInt8,               Lt,                  i8,                      i8_args_)
OP_MACRO(LtInt16,              Lt,                  i16,                     i16_args_)
OP_MACRO(LtInt32,              Lt,                  i32,                     i32_args_)
OP_MACRO(LtInt64,              Lt,                  i64,                     i64_args_)
OP_MACRO(LtNat8,               Lt,                  u8,                      u8_args_)
OP_MACRO(LtNat16,              Lt,                  u16,                     u16_args_)
OP_MACRO(LtNat32,              Lt,                  u32,                     u32_args_)
OP_MACRO(LtNat64,              Lt,                  u64,                     u64_args_)
OP_MACRO(LtFloat32,            Lt,                  float,                   float32_args_)
OP_MACRO(LtFloat64,            Lt,                  double,                  float64_args_)
OP_MACRO(LtFlags,              Lt,                  FlagsVal,                flags_args_)
OP_MACRO(LeInt8,               Le,                  i8,                      i8_args_)
OP_MACRO(LeInt16,              Le,                  i16,                     i16_args_)
OP_MACRO(LeInt32,              Le,                  i32,                     i32_args_)
OP_MACRO(LeInt64,              Le,                  i64,                     i64_args_)
OP_MACRO(LeNat8,               Le,                  u8,                      u8_args_)
OP_MACRO(LeNat16,              Le,                  u16,                     u16_args_)
OP_MACRO(LeNat32,              Le,                  u32,                     u32_args_)
OP_MACRO(LeNat64,              Le,                  u64,                     u64_args_)
OP_MACRO(LeFloat32,            Le,                  float,                   float32_args_)
OP_MACRO(LeFloat64,            Le,                  double,                  float64_args_)
OP_MACRO(LeFlags,              Le,                  FlagsVal,                flags_args_)
OP_MACRO(GtInt8,               Gt,                  i8,                      i8_args_)
OP_MACRO(GtInt16,              Gt,                  i16,                     i16_args_)
OP_MACRO(GtInt32,              Gt,                  i32,                     i32_args_)
OP_MACRO(GtInt64,              Gt,                  i64,                     i64_args_)
OP_MACRO(GtNat8,               Gt,                  u8,                      u8_args_)
OP_MACRO(GtNat16,              Gt,                  u16,                     u16_args_)
OP_MACRO(GtNat32,              Gt,                  u32,                     u32_args_)
OP_MACRO(GtNat64,              Gt,                  u64,                     u64_args_)
OP_MACRO(GtFloat32,            Gt,                  float,                   float32_args_)
OP_MACRO(GtFloat64,            Gt,                  double,                  float64_args_)
OP_MACRO(GtFlags,              Gt,                  FlagsVal,                flags_args_)
OP_MACRO(GeInt8,               Ge,                  i8,                      i8_args_)
OP_MACRO(GeInt16,              Ge,                  i16,                     i16_args_)
OP_MACRO(GeInt32,              Ge,                  i32,                     i32_args_)
OP_MACRO(GeInt64,              Ge,                  i64,                     i64_args_)
OP_MACRO(GeNat8,               Ge,                  u8,                      u8_args_)
OP_MACRO(GeNat16,              Ge,                  u16,                     u16_args_)
OP_MACRO(GeNat32,              Ge,                  u32,                     u32_args_)
OP_MACRO(GeNat64,              Ge,                  u64,                     u64_args_)
OP_MACRO(GeFloat32,            Ge,                  float,                   float32_args_)
OP_MACRO(GeFloat64,            Ge,                  double,                  float64_args_)
OP_MACRO(GeFlags,              Ge,                  FlagsVal,                flags_args_)
OP_MACRO(EqBool,               Eq,                  bool,                    bool_args_)
OP_MACRO(EqInt8,               Eq,                  i8,                      i8_args_)
OP_MACRO(EqInt16,              Eq,                  i16,                     i16_args_)
OP_MACRO(EqInt32,              Eq,                  i32,                     i32_args_)
OP_MACRO(EqInt64,              Eq,                  i64,                     i64_args_)
OP_MACRO(EqNat8,               Eq,                  u8,                      u8_args_)
OP_MACRO(EqNat16,              Eq,                  u16,                     u16_args_)
OP_MACRO(EqNat32,              Eq,                  u32,                     u32_args_)
OP_MACRO(EqNat64,              Eq,                  u64,                     u64_args_)
OP_MACRO(EqFloat32,            Eq,                  float,                   float32_args_)
OP_MACRO(EqFloat64,            Eq,                  double,                  float64_args_)
OP_MACRO(EqType,               Eq,                  type::Type const*,       type_args_)
OP_MACRO(EqEnum,               Eq,                  EnumVal,                 enum_args_)
OP_MACRO(EqFlags,              Eq,                  FlagsVal,                flags_args_)
OP_MACRO(EqAddr,               Eq,                  ir::Addr,                addr_args_)
OP_MACRO(NeInt8,               Ne,                  i8,                      i8_args_)
OP_MACRO(NeInt16,              Ne,                  i16,                     i16_args_)
OP_MACRO(NeInt32,              Ne,                  i32,                     i32_args_)
OP_MACRO(NeInt64,              Ne,                  i64,                     i64_args_)
OP_MACRO(NeNat8,               Ne,                  u8,                      u8_args_)
OP_MACRO(NeNat16,              Ne,                  u16,                     u16_args_)
OP_MACRO(NeNat32,              Ne,                  u32,                     u32_args_)
OP_MACRO(NeNat64,              Ne,                  u64,                     u64_args_)
OP_MACRO(NeFloat32,            Ne,                  float,                   float32_args_)
OP_MACRO(NeFloat64,            Ne,                  double,                  float64_args_)
OP_MACRO(NeType,               Ne,                  type::Type const*,       type_args_)
OP_MACRO(NeEnum,               Ne,                  EnumVal,                 enum_args_)
OP_MACRO(NeFlags,              Ne,                  FlagsVal,                flags_args_)
OP_MACRO(NeAddr,               Ne,                  ir::Addr,                addr_args_)
OP_MACRO(XorBool,              Xor,                 bool,                    bool_args_)
OP_MACRO(XorFlags,             Xor,                 FlagsVal,                flags_args_)
OP_MACRO(OrFlags,              Or,                  FlagsVal,                flags_args_)
OP_MACRO(AndFlags,             And,                 FlagsVal,                flags_args_)
OP_MACRO(CreateStruct,         CreateStruct,        void,                    empty_)
OP_MACRO(CreateStructField,    CreateStructField,   void,                    create_struct_field_)
OP_MACRO(SetStructFieldName,   SetStructFieldName,  void,                    set_struct_field_name_)
OP_MACRO(FinalizeStruct,       Finalize,            type::Struct const*,     reg_)
OP_MACRO(DebugIr,              DebugIr,             void,                    empty_)
OP_MACRO(Alloca,               Alloca,              type::Type const*,       type_)
OP_MACRO(PtrIncr,              PtrIncr,             void,                    ptr_incr_)
OP_MACRO(Field,                Field,               void,                    field_)
OP_MACRO(BufPtr,               BufPtr,              void,                    reg_)
OP_MACRO(Ptr,                  Ptr,                 void,                    reg_)
OP_MACRO(Arrow,                Arrow,               type::Type const*,       type_args_)
OP_MACRO(Array,                Array,               void,                    array_)
OP_MACRO(CreateTuple,          Create,              type::Tuple const*,      empty_)
OP_MACRO(AppendToTuple,        Append,              type::Tuple const*,      store_type_)
OP_MACRO(FinalizeTuple,        Finalize,            type::Tuple const*,      reg_)
OP_MACRO(CreateVariant,        Create,              type::Variant const*,    empty_)
OP_MACRO(AppendToVariant,      Append,              type::Variant const*,    store_type_)
OP_MACRO(FinalizeVariant,      Finalize,            type::Variant const*,    reg_)
OP_MACRO(CreateBlockSeq,       Create,              BlockSequence,           empty_)
OP_MACRO(AppendToBlockSeq,     Append,              BlockSequence,           store_block_)
OP_MACRO(FinalizeBlockSeq,     Finalize,            BlockSequence,           reg_)
OP_MACRO(CondJump,             Jump,                bool,                    cond_jump_)
OP_MACRO(UncondJump,           Jump,                void,                    block_)
OP_MACRO(ReturnJump,           Jump,                i32,  /* any tag */      empty_)
OP_MACRO(BlockSeqJump,         Jump,                BlockSequence,           block_seq_jump_)
OP_MACRO(VariantType,          VariantType,         void,                    reg_)
OP_MACRO(VariantValue,         VariantValue,        void,                    reg_)
OP_MACRO(Call,                 Call,                void,                    call_)
OP_MACRO(CastToInt16,          Cast,                i16,                     typed_reg_)
OP_MACRO(CastToNat16,          Cast,                u16,                     typed_reg_)
OP_MACRO(CastToInt32,          Cast,                i32,                     typed_reg_)
OP_MACRO(CastToNat32,          Cast,                u32,                     typed_reg_)
OP_MACRO(CastToInt64,          Cast,                i64,                     typed_reg_)
OP_MACRO(CastToNat64,          Cast,                u64,                     typed_reg_)
OP_MACRO(CastToFloat32,        Cast,                float,                   typed_reg_)
OP_MACRO(CastToFloat64,        Cast,                double,                  typed_reg_)
OP_MACRO(CastPtr,              CastPtr,             type::Type const*,       typed_reg_)
OP_MACRO(PhiBool,              Phi,                 bool,                    phi_bool_)
OP_MACRO(PhiInt8,              Phi,                 i8,                      phi_i8_)
OP_MACRO(PhiInt16,             Phi,                 i16,                     phi_i16_)
OP_MACRO(PhiInt32,             Phi,                 i32,                     phi_i32_)
OP_MACRO(PhiInt64,             Phi,                 i64,                     phi_i64_)
OP_MACRO(PhiNat8,              Phi,                 u8,                      phi_u8_)
OP_MACRO(PhiNat16,             Phi,                 u16,                     phi_u16_)
OP_MACRO(PhiNat32,             Phi,                 u32,                     phi_u32_)
OP_MACRO(PhiNat64,             Phi,                 u64,                     phi_u64_)
OP_MACRO(PhiFloat32,           Phi,                 float,                   phi_float32_)
OP_MACRO(PhiFloat64,           Phi,                 double,                  phi_float64_)
OP_MACRO(PhiType,              Phi,                 type::Type const*,       phi_type_)
OP_MACRO(PhiBlock,             Phi,                 BlockSequence,           phi_block_)
OP_MACRO(PhiAddr,              Phi,                 ir::Addr,                phi_addr_)
OP_MACRO(BlockSeqContains,     BlockSeqContains,    void,                    block_seq_contains_)
OP_MACRO(GetRet,               GetRet,              void,                    get_ret_)
OP_MACRO(SetRetBool,           SetRet,              bool,                    set_ret_bool_)
OP_MACRO(SetRetInt8,           SetRet,              i8,                      set_ret_i8_)
OP_MACRO(SetRetInt16,          SetRet,              i16,                     set_ret_i16_)
OP_MACRO(SetRetInt32,          SetRet,              i32,                     set_ret_i32_)
OP_MACRO(SetRetInt64,          SetRet,              i64,                     set_ret_i64_)
OP_MACRO(SetRetNat8,           SetRet,              u8,                      set_ret_u8_)
OP_MACRO(SetRetNat16,          SetRet,              u16,                     set_ret_u16_)
OP_MACRO(SetRetNat32,          SetRet,              u32,                     set_ret_u32_)
OP_MACRO(SetRetNat64,          SetRet,              u64,                     set_ret_u64_)
OP_MACRO(SetRetFloat32,        SetRet,              float,                   set_ret_float32_)
OP_MACRO(SetRetFloat64,        SetRet,              double,                  set_ret_float64_)
OP_MACRO(SetRetType,           SetRet,              type::Type const*,       set_ret_type_)
OP_MACRO(SetRetEnum,           SetRet,              EnumVal,                 set_ret_enum_)
OP_MACRO(SetRetFlags,          SetRet,              FlagsVal,                set_ret_flags_)
OP_MACRO(SetRetCharBuf,        SetRet,              std::string_view,        set_ret_char_buf_)
OP_MACRO(SetRetAddr,           SetRet,              ir::Addr,                set_ret_addr_)
OP_MACRO(SetRetFunc,           SetRet,              AnyFunc,                 set_ret_func_)
OP_MACRO(SetRetScope,          SetRet,              ast::ScopeLiteral*,      set_ret_scope_)
OP_MACRO(SetRetGeneric,        SetRet,              ast::FunctionLiteral  *, set_ret_generic_)
OP_MACRO(SetRetModule,         SetRet,              Module const*,           set_ret_module_)
OP_MACRO(SetRetBlock,          SetRet,              BlockSequence,           set_ret_block_)
OP_MACRO(GenerateStruct,       GenerateStruct,      void,                    generate_struct_)
