//
// Created by unite on 12.08.2025.
//

#ifndef NEKOLIB_RULE_OF_FIVE_H
#define NEKOLIB_RULE_OF_FIVE_H

namespace core::test
{
class RuleOfFive
{
    public:
    ~RuleOfFive() = default;
    RuleOfFive(const RuleOfFive& other)
    {
        copy_constructor_called = true;
    }
    RuleOfFive& operator=(const RuleOfFive& other)
    {
        copy_assignment_called = true;
        return *this;
    }
    RuleOfFive(RuleOfFive&& other) noexcept
    {
        move_constructor_called = true;
    }
    RuleOfFive& operator=(RuleOfFive&& other) noexcept
    {
        move_assignment_called = true;
        return *this;
    }

    bool copy_constructor_called = false;
    bool move_constructor_called = false;
    bool copy_assignment_called = false;
    bool move_assignment_called = false;
};
}
#endif //NEKOLIB_RULE_OF_FIVE_H