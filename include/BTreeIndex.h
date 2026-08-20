#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class BTreeIndex{
public:
    bool insert(int64_t key, std::size_t rowNumber);
    bool contains(int64_t key) const;
    std::optional<std::size_t> find(int64_t key) const;
    std::vector<std::pair<int64_t, std::size_t>> orderedEntries() const;
    void clear();
    void save(const std::string& filename) const;
    bool load(const std::string& filename);

private:
    struct Node{
        bool leaf = true;
        std::vector<int64_t> keys;
        std::vector<std::size_t> values;
        std::vector<std::size_t> children;
        std::size_t next = 0;
    };
    struct Split{
        int64_t separator;
        std::size_t right;
    };

    static constexpr std::size_t MAX_KEYS = 4;
    std::vector<Node> nodes_;
    std::size_t root_ = 0;
    std::size_t firstLeaf_ = 0;

    Split* insertInto(std::size_t nodeNumber, int64_t key, std::size_t rowNumber, std::optional<Split>& split);
    std::size_t childFor(const Node& node, int64_t key) const;
};