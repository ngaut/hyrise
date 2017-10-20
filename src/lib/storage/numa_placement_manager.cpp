#if HYRISE_NUMA_SUPPORT

#include "numa_placement_manager.hpp"

#include <numa.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <experimental/memory_resource>

#include "tasks/chunk_metrics_collection_task.hpp"
#include "tasks/migration_preparation_task.hpp"

namespace opossum {

// TODO(normanrz): C++11 singleton http://cppisland.com/?p=501
std::shared_ptr<NUMAPlacementManager> NUMAPlacementManager::_instance;

const std::shared_ptr<NUMAPlacementManager>& NUMAPlacementManager::get() { return _instance; }

void NUMAPlacementManager::set(const std::shared_ptr<NUMAPlacementManager>& instance) { _instance = instance; }

bool NUMAPlacementManager::is_set() { return !!_instance; }

NUMAPlacementManager::NUMAPlacementManager(const std::shared_ptr<Topology> topology,
                                           const NUMAPlacementManagerOptions options)
    : _topology(topology), _options(options) {
  for (size_t i = 0; i < _topology->nodes().size(); i++) {
    char msource_name[8];
    std::snprintf(msource_name, sizeof(msource_name), "numa_%03lu", i);
    memory_resources.push_back(NUMAMemoryResource(i, std::string(msource_name)));
  }

  _collector_thread = std::make_unique<PausableLoopThread>(_options.counter_history_interval, [](size_t) {
    const auto task = std::make_shared<ChunkMetricsCollectionTask>();

    task->schedule();
    task->join();
  });

  _migration_thread = std::make_unique<PausableLoopThread>(_options.migration_interval, [this](size_t) {
    const auto task = std::make_shared<MigrationPreparationTask>(_options);
    task->schedule();
    task->join();
  });
}

const std::experimental::pmr::memory_resource* NUMAPlacementManager::get_memory_resource(int node_id) {
  DebugAssert(node_id > 0 && node_id < static_cast<int>(_topology->nodes().size(), "node_id is out of bounds"));
  return &memory_resources[static_cast<size_t>(node_id)];
}

static int NUMAPlacementManager::get_node_id_of(void* ptr) {
  int status[1];
  void* addr = {ptr};
  numa_move_pages(0, 1, static_cast<void**>(&addr), NULL, reinterpret_cast<int*>(&status), 0);
  return status[0];
}

void NUMAPlacementManager::resume() {
  _collector_thread->resume();
  _migration_thread->resume();
}
void NUMAPlacementManager::pause() {
  _collector_thread->pause();
  _migration_thread->pause();
}

const std::shared_ptr<Topology>& NUMAPlacementManager::topology() const { return _topology; }

}  // namespace opossum

#endif
