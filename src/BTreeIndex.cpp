#include "BTreeIndex.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace {
constexpr std::array<char, 8> MAGIC{'L', 'D', 'B', 'T', 'R', 'E', 'E', '1'};
constexpr uint32_t VERSION = 1;
}

void BTreeIndex::clear(){
    nodes_.clear();
    nodes_.push_back(Node{});
    root_ = 0;
    firstLeaf_ = 0;
}

bool BTreeIndex::insert(int64_t key, std::size_t rowNumber){
    if(nodes_.empty()) clear();
    if(contains(key)) return false;
    std::optional<Split> split;
    insertInto(root_, key, rowNumber, split);
    if(split){
        Node newRoot;
        newRoot.leaf = false;
        newRoot.keys.push_back(split->separator);
        newRoot.children = {root_, split->right};
        nodes_.push_back(std::move(newRoot));
        root_ = nodes_.size() - 1;
    }
    return true;
}

BTreeIndex::Split* BTreeIndex::insertInto(std::size_t nodeNumber, int64_t key, std::size_t rowNumber, std::optional<Split>& split){
    auto& node = nodes_[nodeNumber];
    if(node.leaf){
        const auto position = std::lower_bound(node.keys.begin(), node.keys.end(), key);
        const auto offset = static_cast<std::size_t>(position - node.keys.begin());
        node.keys.insert(position, key);
        node.values.insert(node.values.begin() + offset, rowNumber);
        if(node.keys.size() <= MAX_KEYS) return nullptr;
        Node right;
        right.leaf = true;
        const auto middle = node.keys.size() / 2;
        right.keys.assign(node.keys.begin() + middle, node.keys.end());
        right.values.assign(node.values.begin() + middle, node.values.end());
        node.keys.erase(node.keys.begin() + middle, node.keys.end());
        node.values.erase(node.values.begin() + middle, node.values.end());
        right.next = node.next;
        const auto rightNumber = nodes_.size();
        nodes_.push_back(std::move(right));
        nodes_[nodeNumber].next = rightNumber;
        split = Split{nodes_[rightNumber].keys.front(), rightNumber};
        if(nodeNumber == firstLeaf_ && nodeNumber != root_){
            firstLeaf_ = nodeNumber;
        }
        return &*split;
    }

    const auto child = childFor(node, key);
    std::optional<Split> childSplit;
    insertInto(child, key, rowNumber, childSplit);
    if(!childSplit) return nullptr;
    auto& current = nodes_[nodeNumber];
    const auto childPosition = std::find(current.children.begin(), current.children.end(), child);
    const auto offset = static_cast<std::size_t>(childPosition - current.children.begin());
    current.keys.insert(current.keys.begin() + offset, childSplit->separator);
    current.children.insert(current.children.begin() + offset + 1, childSplit->right);
    if(current.keys.size() <= MAX_KEYS){ split.reset(); return nullptr; }
    Node right;
    right.leaf = false;
    const auto middle = current.keys.size() / 2;
    const auto promoted = current.keys[middle];
    right.keys.assign(current.keys.begin() + middle + 1, current.keys.end());
    right.children.assign(current.children.begin() + middle + 1, current.children.end());
    current.keys.erase(current.keys.begin() + middle, current.keys.end());
    current.children.erase(current.children.begin() + middle + 1, current.children.end());
    nodes_.push_back(std::move(right));
    split = Split{promoted, nodes_.size() - 1};
    return &*split;
}

std::size_t BTreeIndex::childFor(const Node& node, int64_t key) const{
    return static_cast<std::size_t>(std::upper_bound(node.keys.begin(), node.keys.end(), key) - node.keys.begin());
}

std::optional<std::size_t> BTreeIndex::find(int64_t key) const{
    if(nodes_.empty()) return std::nullopt;
    std::size_t nodeNumber = root_;
    while(!nodes_[nodeNumber].leaf) nodeNumber = nodes_[nodeNumber].children[childFor(nodes_[nodeNumber], key)];
    const auto& node = nodes_[nodeNumber];
    const auto position = std::lower_bound(node.keys.begin(), node.keys.end(), key);
    if(position != node.keys.end() && *position == key) return node.values[static_cast<std::size_t>(position - node.keys.begin())];
    for(std::size_t leaf = firstLeaf_; ; leaf = nodes_[leaf].next){
        const auto& candidate = nodes_[leaf];
        const auto candidatePosition = std::lower_bound(candidate.keys.begin(), candidate.keys.end(), key);
        if(candidatePosition != candidate.keys.end() && *candidatePosition == key) return candidate.values[static_cast<std::size_t>(candidatePosition - candidate.keys.begin())];
        if(candidate.next == 0) break;
    }
    return std::nullopt;
}

bool BTreeIndex::contains(int64_t key) const{
    return find(key).has_value();
}

std::vector<std::pair<int64_t, std::size_t>> BTreeIndex::orderedEntries() const{
    std::vector<std::pair<int64_t, std::size_t>> entries;
    if(nodes_.empty()) return entries;
    for(std::size_t leaf = firstLeaf_; leaf != 0 || (leaf == 0 && nodes_[leaf].leaf); leaf = nodes_[leaf].next){
        const auto& node = nodes_[leaf];
        for(std::size_t index = 0; index < node.keys.size(); index++) entries.emplace_back(node.keys[index], node.values[index]);
        if(node.next == 0) break;
    }
    return entries;
}

void BTreeIndex::save(const std::string& filename) const{
    std::ofstream output(filename, std::ios::binary | std::ios::trunc);
    if(!output.is_open()) throw std::runtime_error("Unable to write B-tree index.");
    output.write(MAGIC.data(), MAGIC.size());
    const uint64_t root = root_, firstLeaf = firstLeaf_, nodeCount = nodes_.size();
    output.write(reinterpret_cast<const char*>(&VERSION), sizeof(VERSION));
    output.write(reinterpret_cast<const char*>(&root), sizeof(root));
    output.write(reinterpret_cast<const char*>(&firstLeaf), sizeof(firstLeaf));
    output.write(reinterpret_cast<const char*>(&nodeCount), sizeof(nodeCount));
    for(const auto& node : nodes_){
        const uint8_t leaf = node.leaf ? 1 : 0;
        const uint64_t next = node.next;
        const uint64_t keyCount = node.keys.size();
        output.write(reinterpret_cast<const char*>(&leaf), sizeof(leaf));
        output.write(reinterpret_cast<const char*>(&next), sizeof(next));
        output.write(reinterpret_cast<const char*>(&keyCount), sizeof(keyCount));
        output.write(reinterpret_cast<const char*>(node.keys.data()), static_cast<std::streamsize>(node.keys.size() * sizeof(int64_t)));
        if(node.leaf){
            output.write(reinterpret_cast<const char*>(node.values.data()), static_cast<std::streamsize>(node.values.size() * sizeof(std::size_t)));
        }else{
            output.write(reinterpret_cast<const char*>(node.children.data()), static_cast<std::streamsize>(node.children.size() * sizeof(std::size_t)));
        }
    }
    if(!output) throw std::runtime_error("Failed to write B-tree index.");
}

bool BTreeIndex::load(const std::string& filename){
    std::ifstream input(filename, std::ios::binary);
    if(!input.is_open()) return false;
    std::array<char, 8> magic{};
    uint32_t version = 0;
    uint64_t root = 0, firstLeaf = 0, nodeCount = 0;
    input.read(magic.data(), magic.size());
    input.read(reinterpret_cast<char*>(&version), sizeof(version));
    input.read(reinterpret_cast<char*>(&root), sizeof(root));
    input.read(reinterpret_cast<char*>(&firstLeaf), sizeof(firstLeaf));
    input.read(reinterpret_cast<char*>(&nodeCount), sizeof(nodeCount));
    if(!input || magic != MAGIC || version != VERSION || nodeCount == 0 || root >= nodeCount || firstLeaf >= nodeCount) throw std::runtime_error("B-tree index is corrupt.");
    std::vector<Node> loaded(nodeCount);
    for(auto& node : loaded){
        uint8_t leaf = 0;
        uint64_t next = 0, keyCount = 0;
        input.read(reinterpret_cast<char*>(&leaf), sizeof(leaf));
        input.read(reinterpret_cast<char*>(&next), sizeof(next));
        input.read(reinterpret_cast<char*>(&keyCount), sizeof(keyCount));
        if(!input || leaf > 1 || keyCount > MAX_KEYS) throw std::runtime_error("B-tree index is corrupt.");
        node.leaf = leaf != 0;
        node.next = static_cast<std::size_t>(next);
        node.keys.resize(keyCount);
        input.read(reinterpret_cast<char*>(node.keys.data()), static_cast<std::streamsize>(keyCount * sizeof(int64_t)));
        if(node.leaf){
            node.values.resize(keyCount);
            input.read(reinterpret_cast<char*>(node.values.data()), static_cast<std::streamsize>(keyCount * sizeof(std::size_t)));
        }else{
            node.children.resize(keyCount + 1);
            input.read(reinterpret_cast<char*>(node.children.data()), static_cast<std::streamsize>((keyCount + 1) * sizeof(std::size_t)));
        }
        if(!input || node.next >= nodeCount) throw std::runtime_error("B-tree index is corrupt.");
    }
    nodes_ = std::move(loaded);
    root_ = static_cast<std::size_t>(root);
    firstLeaf_ = static_cast<std::size_t>(firstLeaf);
    return true;
}