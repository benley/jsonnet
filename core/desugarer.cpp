/*
Copyright 2015 Google Inc. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <cassert>

#include "ast.h"
#include "lexer.h"
#include "parser.h"
#include "string_utils.h"

static const Fodder EF;  // Empty fodder.

static const LocationRange E;  // Empty.
    
static unsigned long max_builtin = 24;
BuiltinDecl jsonnet_builtin_decl(unsigned long builtin)
{
    switch (builtin) {
        case 0: return {U"makeArray", {U"sz", U"func"}};
        case 1: return {U"pow", {U"x", U"n"}};
        case 2: return {U"floor", {U"x"}};
        case 3: return {U"ceil", {U"x"}};
        case 4: return {U"sqrt", {U"x"}};
        case 5: return {U"sin", {U"x"}};
        case 6: return {U"cos", {U"x"}};
        case 7: return {U"tan", {U"x"}};
        case 8: return {U"asin", {U"x"}};
        case 9: return {U"acos", {U"x"}};
        case 10: return {U"atan", {U"x"}};
        case 11: return {U"type", {U"x"}};
        case 12: return {U"filter", {U"func", U"arr"}};
        case 13: return {U"objectHasEx", {U"obj", U"f", U"inc_hidden"}};
        case 14: return {U"length", {U"x"}};
        case 15: return {U"objectFieldsEx", {U"obj", U"inc_hidden"}};
        case 16: return {U"codepoint", {U"str"}};
        case 17: return {U"char", {U"n"}};
        case 18: return {U"log", {U"n"}};
        case 19: return {U"exp", {U"n"}};
        case 20: return {U"mantissa", {U"n"}};
        case 21: return {U"exponent", {U"n"}};
        case 22: return {U"modulo", {U"a", U"b"}};
        case 23: return {U"extVar", {U"x"}};
        case 24: return {U"primitiveEquals", {U"a", U"b"}};
        default:
        std::cerr << "INTERNAL ERROR: Unrecognized builtin function: " << builtin << std::endl;
        std::abort();
    }
    // Quiet, compiler.
    return BuiltinDecl();
}

static constexpr char STD_CODE[] = {
    #include "std.jsonnet.h"
};

/** Desugar Jsonnet expressions to reduce the number of constructs the rest of the implementation
 * needs to understand.
 *
 * Desugaring should happen immediately after parsing, i.e. before static analysis and execution.
 * Temporary variables introduced here should be prefixed with $ to ensure they do not clash with
 * variables used in user code.
 */
class Desugarer {

    Allocator *alloc;

    template <class T, class... Args> T* make(Args&&... args)
    {
        return alloc->make<T>(std::forward<Args>(args)...);
    }

    const Identifier *id(const String &s)
    { return alloc->makeIdentifier(s); }

    LiteralString *str(const String &s)
    { return make<LiteralString>(E, EF, s, LiteralString::DOUBLE, "", ""); }

    LiteralString *str(const LocationRange &loc, const String &s)
    { return make<LiteralString>(loc, EF, s, LiteralString::DOUBLE, "", ""); }

    Var *var(const Identifier *ident)
    { return make<Var>(E, EF, ident); }

    Var *std(void)
    { return var(id(U"std")); }

    
    Local::Bind bind(const Identifier *id, AST *body)
    {
        return Local::Bind(EF, id, EF, body, false, EF, Params{}, false, EF, EF);
    }

    Local::Binds singleBind(const Identifier *id, AST *body)
    {
        return {bind(id, body)};
    }

    Array *singleton(AST *body)
    {
        return make<Array>(body->location, EF, Array::Elements{Array::Element(body, EF)},
                           false, EF);
    }

    Apply *stdFunc(const String &name, AST *v)
    {
        return make<Apply>(
            v->location, 
            EF,
            make<Index>(E, EF, std(), EF, false, str(name), EF, nullptr, EF, nullptr, EF),
            EF,
            Apply::Args{{v, EF}},
            false,  // trailingComma
            EF,
            EF,
            true  // tailstrict
        );
    }

    Apply *stdFunc(const LocationRange &loc, const String &name, AST *a, AST *b)
    {
        return make<Apply>(
            loc, 
            EF,
            make<Index>(E, EF, std(), EF, false, str(name), EF, nullptr, EF, nullptr, EF),
            EF,
            Apply::Args{{a, EF}, {b, EF}},
            false,  // trailingComma
            EF,
            EF,
            true  // tailstrict
        );
    }

    Apply *length(AST *v)
    {
        return stdFunc(U"length", v);
    }

    Apply *type(AST *v)
    {
        return stdFunc(U"type", v);
    }

    Apply *equals(const LocationRange &loc, AST *a, AST *b)
    {
        return stdFunc(loc, U"equals", a, b);
    }

    Error *error(AST *msg)
    {
        return alloc->make<Error>(msg->location, EF, msg);
    }

    Error *error(const LocationRange &loc, const String &msg)
    {
        return error(str(loc, msg));
    }

    public:
    Desugarer(Allocator *alloc)
      : alloc(alloc)
    { }

    void desugarFields(AST *ast, ObjectFields &fields, unsigned obj_level)
    {
        // Desugar children
        for (auto &field : fields) {
            if (field.expr1 != nullptr) desugar(field.expr1, obj_level);
            desugar(field.expr2, obj_level + 1);
            if (field.expr3 != nullptr) desugar(field.expr3, obj_level + 1);
        }

        // Simplify asserts
        for (auto &field : fields) {
            if (field.kind != ObjectField::ASSERT) continue;
            AST *msg = field.expr3;
            field.expr3 = nullptr;
            if (msg == nullptr) {
                auto msg_str = U"Object assertion failed.";
                msg = alloc->make<LiteralString>(field.expr2->location, EF, msg_str,
                                                 LiteralString::DOUBLE, "", "");
            }

            // if expr2 then true else error msg
            field.expr2 = alloc->make<Conditional>(
                ast->location,
                EF,
                field.expr2,
                EF,
                alloc->make<LiteralBoolean>(E, EF, true),
                EF,
                error(msg));
        }

        // Remove methods
        for (auto &field : fields) {
            if (!field.methodSugar) continue;
            field.expr2 = alloc->make<Function>(
                field.expr2->location, EF, field.fodderL, field.params, field.trailingComma,
                field.fodderR, field.expr2);
            field.methodSugar = false;
            field.params.clear();
        }


        // Remove object-level locals
        auto copy = fields;
        fields.clear();
        Local::Binds binds;
        for (auto &local : copy) {
            if (local.kind != ObjectField::LOCAL) continue;
            binds.push_back(bind(local.id, local.expr2));
        }
        for (auto &field : copy) {
            if (field.kind == ObjectField::LOCAL) continue;
            if (!binds.empty())
                field.expr2 = alloc->make<Local>(field.expr2->location, EF, binds, field.expr2);
            fields.push_back(field);
        }

        // Change all to FIELD_EXPR
        for (auto &field : fields) {
            switch (field.kind) {
                case ObjectField::ASSERT:
                // Nothing to do.
                break;

                case ObjectField::FIELD_ID:
                field.expr1 = str(field.id->name);
                field.kind = ObjectField::FIELD_EXPR;
                break;

                case ObjectField::FIELD_EXPR:
                // Nothing to do.
                break;

                case ObjectField::FIELD_STR:
                // Just set the flag.
                field.kind = ObjectField::FIELD_EXPR;
                break;

                case ObjectField::LOCAL:
                std::cerr << "Locals should be removed by now." << std::endl;
                abort();
            }
        }

        // Remove +:
        for (auto &field : fields) {
            if (!field.superSugar) continue;
            AST *super_f = alloc->make<SuperIndex>(field.expr1->location, EF, EF, field.expr1, EF,
                                                   nullptr);
            field.expr2 = alloc->make<Binary>(ast->location, EF, super_f, EF, BOP_PLUS,
                                              field.expr2);
            field.superSugar = false;
        }
    }

    void desugar(AST *&ast_, unsigned obj_level)
    {
        if (auto *ast = dynamic_cast<Apply*>(ast_)) {
            desugar(ast->target, obj_level);
            for (Apply::Arg &arg : ast->args)
                desugar(arg.expr, obj_level);

        } else if (auto *ast = dynamic_cast<ApplyBrace*>(ast_)) {
            desugar(ast->left, obj_level);
            desugar(ast->right, obj_level);
            ast_ = alloc->make<Binary>(ast->location, ast->openFodder,
                                       ast->left, EF, BOP_PLUS, ast->right);

        } else if (auto *ast = dynamic_cast<Array*>(ast_)) {
            for (auto &el : ast->elements)
                desugar(el.expr, obj_level);

        } else if (auto *ast = dynamic_cast<ArrayComprehension*>(ast_)) {
            for (ComprehensionSpec &spec : ast->specs)
                desugar(spec.expr, obj_level);
            desugar(ast->body, obj_level + 1);

            int n = ast->specs.size();
            AST *zero = make<LiteralNumber>(E, EF, "0.0");
            AST *one = make<LiteralNumber>(E, EF, "1.0");
            auto *_r = id(U"$r");
            auto *_l = id(U"$l");
            std::vector<const Identifier*> _i(n);
            for (int i = 0; i < n ; ++i) {
                StringStream ss;
                ss << U"$i_" << i;
                _i[i] = id(ss.str());
            }
            std::vector<const Identifier*> _aux(n);
            for (int i = 0; i < n ; ++i) {
                StringStream ss;
                ss << U"$aux_" << i;
                _aux[i] = id(ss.str());
            }

            // Build it from the inside out.  We keep wrapping 'in' with more ASTs.
            assert(ast->specs[0].kind == ComprehensionSpec::FOR);

            int last_for = n - 1;
            while (ast->specs[last_for].kind != ComprehensionSpec::FOR)
                last_for--;
            // $aux_{last_for}($i_{last_for} + 1, $r + [body])
            AST *in = make<Apply>(
                ast->body->location,
                EF,
                var(_aux[last_for]),
                EF,
                Apply::Args {
                    { make<Binary>(E, EF, var(_i[last_for]), EF, BOP_PLUS, one), EF},
                    { make<Binary>(E, EF, var(_r), EF, BOP_PLUS, singleton(ast->body)), EF}
                },
                false,  // trailingComma
                EF,
                EF,
                true  // tailstrict
            );
            for (int i = n - 1; i >= 0 ; --i) {
                const ComprehensionSpec &spec = ast->specs[i];
                AST *out;
                if (i > 0) {
                    int prev_for = i - 1;
                    while (ast->specs[prev_for].kind != ComprehensionSpec::FOR)
                        prev_for--;

                    // aux_{prev_for}($i_{prev_for} + 1, $r)
                    out = make<Apply>(  // False branch.
                        E,
                        EF,
                        var(_aux[prev_for]), 
                        EF,
                        Apply::Args {
                            { make<Binary>(E, EF, var(_i[prev_for]), EF, BOP_PLUS, one), EF, },
                            { var(_r), EF, }
                        },
                        false, // trailingComma
                        EF,
                        EF,
                        true  // tailstrict
                    );
                } else {
                    out = var(_r);
                }
                switch (spec.kind) {
                    case ComprehensionSpec::IF: {
                        /*
                            if [[[...cond...]]] then
                                [[[...in...]]]
                            else
                                [[[...out...]]]
                        */
                        in = make<Conditional>(
                            ast->location,
                            EF,
                            spec.expr,
                            EF,
                            in,  // True branch.
                            EF,
                            out);  // False branch.
                    } break;
                    case ComprehensionSpec::FOR: {
                        /*
                            local $l = [[[...array...]]]
                                  aux_{i}(i_{i}, r) =
                                if i_{i} >= std.length($l) then
                                    [[[...out...]]]
                                else
                                    local [[[...var...]]] = $l[i_{i}];
                                    [[[...in...]]];`
                            if std.type($l) != "array" then
                                error "In comprehension, can only iterate over array.."
                            else
                                aux_{i}(0, r) tailstrict;
                        */
                        in = make<Local>(
                            ast->location,
                            EF,
                            Local::Binds {
                                bind(_l, spec.expr),  // Need to check expr is an array
                                bind(_aux[i], make<Function>(
                                    ast->location,
                                    EF,
                                    EF,
                                    std::vector<Param>{Param(EF, _i[i], EF), Param(EF, _r, EF)},
                                    false,  // trailingComma
                                    EF,
                                    make<Conditional>(
                                        ast->location, 
                                        EF,
                                        make<Binary>(
                                            E, EF, var(_i[i]), EF, BOP_GREATER_EQ, length(var(_l))),
                                        EF,
                                        out,
                                        EF,
                                        make<Local>(
                                            ast->location,
                                            EF,
                                            singleBind(
                                                spec.var,
                                                make<Index>(E, EF, var(_l), EF, false, var(_i[i]),
                                                            EF, nullptr, EF, nullptr, EF)
                                            ),
                                            in)
                                    )
                                ))},
                            make<Conditional>(
                                ast->location, 
                                EF,
                                equals(ast->location, type(var(_l)), str(U"array")),
                                EF,
                                make<Apply>(
                                    E,
                                    EF,
                                    var(_aux[i]),
                                    EF,
                                    Apply::Args {
                                        {zero, EF},
                                        {
                                            i == 0
                                            ? make<Array>(E, EF, Array::Elements{}, false, EF)
                                            : static_cast<AST*>(var(_r)),
                                            EF,
                                        }
                                    },
                                    false,  // trailingComma
                                    EF,
                                    EF,
                                    true),  // tailstrict
                                EF,
                                error(ast->location,
                                      U"In comprehension, can only iterate over array.")));
                    } break;
                }
            }

            ast_ = in;

        } else if (auto *ast = dynamic_cast<Assert*>(ast_)) {
            desugar(ast->cond, obj_level);
            if (ast->message == nullptr) {
                ast->message = str(U"Assertion failed.");
            }
            desugar(ast->message, obj_level);
            desugar(ast->rest, obj_level);

            // if cond then rest else error msg
            AST *branch_false = alloc->make<Error>(ast->location, EF, ast->message);
            ast_ = alloc->make<Conditional>(ast->location, ast->openFodder,
                                            ast->cond, EF, ast->rest, EF, branch_false);

        } else if (auto *ast = dynamic_cast<Binary*>(ast_)) {
            desugar(ast->left, obj_level);
            desugar(ast->right, obj_level);

            bool invert = false;

            switch (ast->op) {
                case BOP_PERCENT: {
                    AST *f_mod = alloc->make<Index>(E, EF, std(), EF, false, str(U"mod"), EF,
                                                    nullptr, EF, nullptr, EF);
                    Apply::Args args = {{ast->left, EF}, {ast->right, EF}};
                    ast_ = alloc->make<Apply>(ast->location, ast->openFodder, f_mod, EF, args,
                                              false, EF, EF, false);
                } break;

                case BOP_MANIFEST_UNEQUAL:
                invert = true;
                case BOP_MANIFEST_EQUAL: {
                    ast_ = equals(ast->location, ast->left, ast->right);
                    if (invert)
                        ast_ = alloc->make<Unary>(ast->location, ast->openFodder, UOP_NOT, ast_);
                }
                break;

                default:;
                // Otherwise don't change it.
            }

        } else if (dynamic_cast<const BuiltinFunction*>(ast_)) {
            // Nothing to do.

        } else if (auto *ast = dynamic_cast<Conditional*>(ast_)) {
            desugar(ast->cond, obj_level);
            desugar(ast->branchTrue, obj_level);
            if (ast->branchFalse == nullptr)
                ast->branchFalse = alloc->make<LiteralNull>(LocationRange(), EF);
            desugar(ast->branchFalse, obj_level);

        } else if (auto *ast = dynamic_cast<Dollar*>(ast_)) {
            if (obj_level == 0) {
                throw StaticError(ast->location, "No top-level object found.");
            }
            ast_ = alloc->make<Var>(ast->location, EF, alloc->makeIdentifier(U"$"));

        } else if (auto *ast = dynamic_cast<Error*>(ast_)) {
            desugar(ast->expr, obj_level);

        } else if (auto *ast = dynamic_cast<Function*>(ast_)) {
            desugar(ast->body, obj_level);

        } else if (dynamic_cast<const Import*>(ast_)) {
            // Nothing to do.

        } else if (dynamic_cast<const Importstr*>(ast_)) {
            // Nothing to do.

        } else if (auto *ast = dynamic_cast<Index*>(ast_)) {
            desugar(ast->target, obj_level);
            if (ast->isSlice) {
                if (ast->index == nullptr)
                    ast->index = make<LiteralNull>(ast->location, EF);
                desugar(ast->index, obj_level);

                if (ast->end == nullptr)
                    ast->end = make<LiteralNull>(ast->location, EF);
                desugar(ast->end, obj_level);

                if (ast->step == nullptr)
                    ast->step = make<LiteralNull>(ast->location, EF);
                desugar(ast->step, obj_level);

                ast_ = make<Apply>(
                    ast->location,
                    EF,
                    make<Index>(
                        E, EF, std(), EF, false, str(U"slice"), EF, nullptr, EF, nullptr, EF),
                    EF,
                    std::vector<Apply::Arg>{
                        {ast->target, EF},
                        {ast->index, EF},
                        {ast->end, EF},
                        {ast->step, EF},
                    },
                    false,  // trailing comma
                    EF,
                    EF,
                    false  // tailstrict
                );
            } else {
                if (ast->id != nullptr) {
                    assert(ast->index == nullptr);
                    ast->index = str(ast->id->name);
                    ast->id = nullptr;
                }
                desugar(ast->index, obj_level);
            }

        } else if (auto *ast = dynamic_cast<Local*>(ast_)) {
            for (auto &bind: ast->binds)
                desugar(bind.body, obj_level);
            desugar(ast->body, obj_level);

            for (auto &bind: ast->binds) {
                if (bind.functionSugar) {
                    bind.body = alloc->make<Function>(
                        ast->location, ast->openFodder, bind.parenLeftFodder, bind.params, false,
                        bind.parenRightFodder, bind.body);
                    bind.functionSugar = false;
                    bind.params.clear();
                }
            }

        } else if (dynamic_cast<const LiteralBoolean*>(ast_)) {
            // Nothing to do.

        } else if (dynamic_cast<const LiteralNumber*>(ast_)) {
            // Nothing to do.

        } else if (auto *ast = dynamic_cast<LiteralString*>(ast_)) {
            ast->value = jsonnet_string_unescape(ast->location, ast->value);
            ast->tokenKind = LiteralString::DOUBLE;
            ast->blockIndent.clear();

        } else if (dynamic_cast<const LiteralNull*>(ast_)) {
            // Nothing to do.

        } else if (auto *ast = dynamic_cast<DesugaredObject*>(ast_)) {
            for (auto &field : ast->fields) {
                desugar(field.name, obj_level);
                desugar(field.body, obj_level + 1);
            }
            for (AST *assert : ast->asserts) {
                desugar(assert, obj_level + 1);
            }

        } else if (auto *ast = dynamic_cast<Object*>(ast_)) {
            // Hidden variable to allow outer/top binding.
            if (obj_level == 0) {
                const Identifier *hidden_var = alloc->makeIdentifier(U"$");
                auto *body = alloc->make<Self>(E, EF);
                ast->fields.push_back(ObjectField::Local(EF, EF, hidden_var, EF, body, EF));
            }

            desugarFields(ast, ast->fields, obj_level);

            DesugaredObject::Fields new_fields;
            ASTs new_asserts;
            for (const ObjectField &field : ast->fields) {
                if (field.kind == ObjectField::ASSERT) {
                    new_asserts.push_back(field.expr2);
                } else if (field.kind == ObjectField::FIELD_EXPR) {
                    new_fields.emplace_back(field.hide, field.expr1, field.expr2);
                } else {
                    std::cerr << "INTERNAL ERROR: field should have been desugared: "
                              << field.kind << std::endl;
                }
            }
            ast_ = alloc->make<DesugaredObject>(ast->location, new_asserts, new_fields);

        } else if (auto *ast = dynamic_cast<ObjectComprehension*>(ast_)) {
            // Hidden variable to allow outer/top binding.
            if (obj_level == 0) {
                const Identifier *hidden_var = alloc->makeIdentifier(U"$");
                auto *body = alloc->make<Self>(E, EF);
                ast->fields.push_back(ObjectField::Local(EF, EF, hidden_var, EF, body, EF));
            }

            desugarFields(ast, ast->fields, obj_level);

            for (ComprehensionSpec &spec : ast->specs)
                desugar(spec.expr, obj_level);

            AST *field = ast->fields.front().expr1;
            AST *value = ast->fields.front().expr2;

            /*  {
                    [arr[0]]: local x = arr[1], y = arr[2], z = arr[3]; val_expr
                    for arr in [ [key_expr, x, y, z] for ...  ]
                }
            */
            auto *_arr = id(U"$arr");
            AST *zero = make<LiteralNumber>(E, EF, "0.0");
            int counter = 1;
            Local::Binds binds;
            Array::Elements arr_e {Array::Element(field, EF)};
            for (ComprehensionSpec &spec : ast->specs) {
                if (spec.kind == ComprehensionSpec::FOR) {
                    std::stringstream num;
                    num << counter++;
                    binds.push_back(bind(
                        spec.var,
                        make<Index>(E, EF, var(_arr), EF, false,
                                    make<LiteralNumber>(E, EF, num.str()), EF, nullptr, EF, nullptr,
                                    EF)));
                    arr_e.emplace_back(var(spec.var), EF);
                }
            }
            AST *arr = make<ArrayComprehension>(
                ast->location,
                EF,
                make<Array>(ast->location, EF, arr_e, false, EF),
                EF,
                false,
                ast->specs,
                EF);
            desugar(arr, obj_level);
            ast_ = make<ObjectComprehensionSimple>(
                ast->location,
                make<Index>(E, EF, var(_arr), EF, false, zero, EF, nullptr, EF, nullptr, EF),
                make<Local>(
                    ast->location,
                    EF,
                    binds,
                    value),
                _arr,
                arr);

        } else if (auto *ast = dynamic_cast<ObjectComprehensionSimple*>(ast_)) {
            desugar(ast->field, obj_level);
            desugar(ast->value, obj_level + 1);
            desugar(ast->array, obj_level);

        } else if (auto *ast = dynamic_cast<Parens*>(ast_)) {
            // Strip parens.
            desugar(ast->expr, obj_level);
            ast_ = ast->expr;

        } else if (dynamic_cast<const Self*>(ast_)) {
            // Nothing to do.

        } else if (auto * ast = dynamic_cast<SuperIndex*>(ast_)) {
            if (ast->id != nullptr) {
                assert(ast->index == nullptr);
                ast->index = str(ast->id->name);
                ast->id = nullptr;
            }
            desugar(ast->index, obj_level);

        } else if (auto *ast = dynamic_cast<Unary*>(ast_)) {
            desugar(ast->expr, obj_level);

        } else if (dynamic_cast<const Var*>(ast_)) {
            // Nothing to do.

        } else {
            std::cerr << "INTERNAL ERROR: Unknown AST: " << ast_ << std::endl;
            std::abort();

        }
    }

    void desugarFile(AST *&ast)
    {
        desugar(ast, 0);

        // Now, implement the std library by wrapping in a local construct.
        Tokens tokens = jsonnet_lex("std.jsonnet", STD_CODE);
        AST *std_ast = jsonnet_parse(alloc, tokens);
        desugar(std_ast, 0);
        auto *std_obj = dynamic_cast<DesugaredObject*>(std_ast);
        if (std_obj == nullptr) {
            std::cerr << "INTERNAL ERROR: std.jsonnet not an object." << std::endl;
            std::abort();
        }

        // Bind 'std' builtins that are implemented natively.
        DesugaredObject::Fields &fields = std_obj->fields;
        for (unsigned long c=0 ; c <= max_builtin ; ++c) {
            const auto &decl = jsonnet_builtin_decl(c);
            Identifiers params;
            for (const auto &p : decl.params)
                params.push_back(alloc->makeIdentifier(p));
            fields.emplace_back(
                ObjectField::HIDDEN,
                str(decl.name),
                alloc->make<BuiltinFunction>(E, c, params));
        }
        fields.emplace_back(
            ObjectField::HIDDEN,
            str(U"thisFile"),
            str(decode_utf8(ast->location.file)));

        ast = alloc->make<Local>(ast->location, EF, singleBind(id(U"std"), std_obj), ast);
    }
};

void jsonnet_desugar(Allocator *alloc, AST *&ast)
{
    Desugarer desugarer(alloc);
    desugarer.desugarFile(ast);
}