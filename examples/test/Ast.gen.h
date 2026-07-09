#pragma once
// AUTO-GENERATED from ast.spec by gen_ast.py — do not edit by hand.
#include <memory>
#include <string>
#include <vector>

namespace Sel
{
    // ---- forward declarations ----
    struct Fixture;
    struct FixtureRange;
    struct Group;
    struct Preset;
    struct SelectCmd;
    struct StoreCmd;
    struct DeleteCmd;
    struct ClearCmd;
    struct AtCmd;

    // ---- visitor interfaces (one per category) ----
    struct SelectorVisitor
    {
        virtual ~SelectorVisitor() = default;
        virtual void visit(Fixture &) = 0;
        virtual void visit(FixtureRange &) = 0;
        virtual void visit(Group &) = 0;
        virtual void visit(Preset &) = 0;
    };

    struct CommandVisitor
    {
        virtual ~CommandVisitor() = default;
        virtual void visit(SelectCmd &) = 0;
        virtual void visit(StoreCmd &) = 0;
        virtual void visit(DeleteCmd &) = 0;
        virtual void visit(ClearCmd &) = 0;
        virtual void visit(AtCmd &) = 0;
    };

    // ---- category bases + smart-pointer aliases ----
    struct Selector
    {
        virtual ~Selector() = default;
        virtual void accept(SelectorVisitor &v) = 0;
    };
    using SelectorPtr = std::unique_ptr<Selector>;

    struct Command
    {
        virtual ~Command() = default;
        virtual void accept(CommandVisitor &v) = 0;
    };
    using CommandPtr = std::unique_ptr<Command>;

    // ---- records (plain structs, not visited) ----
    struct AtValue
    {
        bool isPreset = false;
        double level = 0;
        SelectorPtr preset;
    };

    struct Item
    {
        std::string op;
        SelectorPtr sel;
    };

    // ---- concrete nodes ----
    struct Fixture : Selector
    {
        long long id = 0;
        void accept(SelectorVisitor &v) override { v.visit(*this); }
    };

    struct FixtureRange : Selector
    {
        long long from = 0;
        long long to = 0;
        void accept(SelectorVisitor &v) override { v.visit(*this); }
    };

    struct Group : Selector
    {
        long long id = 0;
        void accept(SelectorVisitor &v) override { v.visit(*this); }
    };

    struct Preset : Selector
    {
        long long bank = 0;
        long long number = 0;
        void accept(SelectorVisitor &v) override { v.visit(*this); }
    };

    struct SelectCmd : Command
    {
        std::vector<Item> items;
        bool hasAt = false;
        AtValue at;
        void accept(CommandVisitor &v) override { v.visit(*this); }
    };

    struct StoreCmd : Command
    {
        SelectorPtr target;
        void accept(CommandVisitor &v) override { v.visit(*this); }
    };

    struct DeleteCmd : Command
    {
        SelectorPtr target;
        void accept(CommandVisitor &v) override { v.visit(*this); }
    };

    struct ClearCmd : Command
    {
        void accept(CommandVisitor &v) override { v.visit(*this); }
    };

    struct AtCmd : Command
    {
        AtValue at;
        void accept(CommandVisitor &v) override { v.visit(*this); }
    };

    using Program = std::vector<CommandPtr>;
}
