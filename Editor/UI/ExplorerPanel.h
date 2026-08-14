#pragma once
#include <memory>
#include <string>
#include <map>

class Instance;

enum class GitStatus {
    None,
    Modified,
    Added,
    Deleted
};

class ExplorerPanel {
public:
    void draw();

    // Git Status Support
    static void setGitStatus(const Instance* inst, GitStatus status);
    static GitStatus getGitStatus(const Instance* inst);

private:
    void drawInstanceNode(const std::shared_ptr<Instance>& inst, int depth = 0);
    void drawInsertObjectPopup();
    void createAndInsertObject(const std::string& className, const std::shared_ptr<Instance>& parent);
    void duplicateInstance(const std::shared_ptr<Instance>& inst);

    std::shared_ptr<Instance> m_insertTarget;
    bool m_openInsertPopup = false;
    char m_insertSearch[64] = "";
};
