#include "UndoStack.h"

void UndoStack::pushPropertyChangeCommand(std::shared_ptr<Instance> inst, std::string prop, std::any oldVal, std::any newVal) {
    push(std::make_unique<PropertyChangeCommand>(inst, prop, oldVal, newVal));
}

void UndoStack::pushCreateCommand(std::shared_ptr<Instance> inst, std::shared_ptr<Instance> parent) {
    auto cmd = std::make_unique<CreateInstanceCommand>(inst, parent);
    cmd->execute(); // Execute immediately upon creation
    push(std::move(cmd));
}

void UndoStack::push(std::unique_ptr<ICommand> cmd) {
    undoList.push_back(std::move(cmd));
    redoList.clear(); // A new action invalidates the redo history
}

void UndoStack::undo() {
    if (undoList.empty()) return;
    undoList.back()->undo();
    redoList.push_back(std::move(undoList.back()));
    undoList.pop_back();
}

void UndoStack::redo() {
    if (redoList.empty()) return;
    redoList.back()->execute();
    undoList.push_back(std::move(redoList.back()));
    redoList.pop_back();
}
