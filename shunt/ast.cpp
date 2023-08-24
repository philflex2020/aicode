#include <iostream>
#include <stack>
#include <string>
#include <memory>
#include <unordered_map>


struct Node {
    virtual ~Node() = default;
};

struct Operator : Node {
    char op;
    std::shared_ptr<Node> left;
    std::shared_ptr<Node> right;

    Operator(char op, std::shared_ptr<Node> left, std::shared_ptr<Node> right)
        : op(op), left(left), right(right) {}
};

struct Operand : Node {
    double value;

    Operand(double value) : value(value) {}
};

struct Variable : Node {
    std::string name;

    Variable(const std::string &name) : name(name) {}
};

std::unordered_map<std::string, double> variables = { {"myvar", 5.1} };


bool isOperator(char c) {
    return c == '+' || c == '-' || c == '*' || c == '/';
}
int precedence(char c) {
    switch(c) {
        case '+':
        case '-':
            return 1;
        case '*':
        case '/':
            return 2;
    }
    return -1;
}


double evaluate(const std::shared_ptr<Node>& node) {
    if (auto operand = std::dynamic_pointer_cast<Operand>(node)) {
        return operand->value;
    } else if (auto variable = std::dynamic_pointer_cast<Variable>(node)) {
        return variables[variable->name];
    } else if (auto op = std::dynamic_pointer_cast<Operator>(node)) {
        double left = evaluate(op->left);
        double right = evaluate(op->right);

        switch (op->op) {
            case '+':
                return left + right;
            case '-':
                return left - right;
            case '*':
                return left * right;
            case '/':
                return left / right; // Note: We are not handling division by zero here
        }
    }
    return 0; // Should not reach here, consider adding error handling
}

std::shared_ptr<Node> infixToAST(const std::string &infix) {
    std::stack<char> operators;
    std::stack<std::shared_ptr<Node>> operands;

    //for (char c : infix) {

    for (size_t i = 0; i < infix.size(); ++i) {
        char c = infix[i];

       
        if (std::isdigit(c) || c == '.') {
            size_t start = i;
            while (i < infix.size() && (std::isdigit(infix[i]) || infix[i] == '.')) ++i;
            std::string numberStr = infix.substr(start, i - start);
            operands.push(std::make_shared<Operand>(std::stod(numberStr)));
            --i; // Decrement i since the for-loop will increment it again

        } else if (c == '{') {
            size_t end = infix.find('}', i);
            if (end != std::string::npos) {
                std::string varName = infix.substr(i + 1, end - i - 1);
                operands.push(std::make_shared<Variable>(varName));
                i = end; // Jump to the end of the variable name
            }
        } else if (isOperator(c)) {
            while (!operators.empty() && precedence(c) <= precedence(operators.top())) {
                char op = operators.top();
                operators.pop();

                auto right = operands.top();
                operands.pop();
                auto left = operands.top();
                operands.pop();

                operands.push(std::make_shared<Operator>(op, left, right));
            }
            operators.push(c);
        } else if (c == '(') {
            operators.push(c);
        } else if (c == ')') {
            while (!operators.empty() && operators.top() != '(') {
                char op = operators.top();
                operators.pop();

                auto right = operands.top();
                operands.pop();
                auto left = operands.top();
                operands.pop();

                operands.push(std::make_shared<Operator>(op, left, right));
            }
            if (!operators.empty() && operators.top() == '(') {
                operators.pop();
            }
        }
    }

    while (!operators.empty()) {
        char op = operators.top();
        operators.pop();

        auto right = operands.top();
        operands.pop();
        auto left = operands.top();
        operands.pop();

        operands.push(std::make_shared<Operator>(op, left, right));
    }

    return operands.top();
}


void printAST(const std::shared_ptr<Node> &node) {
    if (auto operand = std::dynamic_pointer_cast<Operand>(node)) {
        std::cout << operand->value;
    } else if (auto variable = std::dynamic_pointer_cast<Variable>(node)) {
        std::cout << '{' << variable->name << '}';
    } else if (auto op = std::dynamic_pointer_cast<Operator>(node)) {
        std::cout << '(';
        printAST(op->left);
        std::cout << ' ' << op->op << ' ';
        printAST(op->right);
        std::cout << ')';
    }
}
int main() {
    std::string infix = "3.5+{myvar}*(2-1)";
    auto ast = infixToAST(infix);
    std::cout << "Infix: " << infix << "\nAST: ";
    printAST(ast);
    std::cout << std::endl;
    double result = evaluate(ast);
    std::cout << "\nResult: " << result << std::endl;
    return 0;
}
