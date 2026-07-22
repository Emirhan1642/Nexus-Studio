#pragma once
#include "Command.h"
#include <vector>
#include <memory>

class UndoStack {
public:
    static UndoStack& instance() {
        static UndoStack s;
        return s;
    }

    void pushPropertyChangeCommand(std::shared_ptr<Instance> inst, std::string prop, std::any oldVal, std::any newVal);
    void pushCreateCommand(std::shared_ptr<Instance> inst, std::shared_ptr<Instance> parent);

    void push(std::unique_ptr<ICommand> cmd);

    void undo();
    void redo();

private:
    UndoStack() = default;

    std::vector<std::unique_ptr<ICommand>> undoList;
    std::vector<std::unique_ptr<ICommand>> redoList;
};
