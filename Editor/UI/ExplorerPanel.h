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
    void drawBatchRenameModal();
    std::string computeRenamedString(const std::string& origName, int index, int totalCount, const std::string& matchPattern, const std::string& renameTemplate, int startNum = 1);
    void createAndInsertObject(const std::string& className, const std::shared_ptr<Instance>& parent);
    std::shared_ptr<Instance> duplicateInstance(const std::shared_ptr<Instance>& inst);

    std::shared_ptr<Instance> m_insertTarget;
    bool m_openInsertPopup = false;
    char m_insertSearch[64] = "";
    int m_insertSelectedIndex = 0;

    bool m_openBatchRenameModal = false;
    char m_renameMatchBuf[128] = "";
    char m_renameToBuf[128] = "";
    int m_startAscendingFrom = 1;
};
