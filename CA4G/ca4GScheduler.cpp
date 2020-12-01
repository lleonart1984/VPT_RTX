#include "ca4G_Private.h"


namespace CA4G {
	GPUScheduler::CommandQueueManager::CommandQueueManager(DX_Device device, D3D12_COMMAND_LIST_TYPE type) : type(type) {
		D3D12_COMMAND_QUEUE_DESC d = {};
		d.Type = type;
		device->CreateCommandQueue(&d, IID_PPV_ARGS(&dxQueue));

		fenceValue = 1;
		device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));

		fenceEvent = CreateEventW(nullptr, false, false, nullptr);
	}

	GPUScheduler::GPUScheduler(gObj<DeviceManager> manager, bool useFrameBuffering, int max_threads, int buffers)
		:
		manager(manager),
		useFrameBuffer(useFrameBuffering)
	{
		DX_Device device = manager->device;

		// Creating the engines (engines are shared by all frames and threads...)
		for (int e = 0; e < CA4G_SUPPORTED_ENGINES; e++) // 0 - direct, 1 - bunddle, 2 - compute, 3 - copy.
		{
			D3D12_COMMAND_LIST_TYPE type = (D3D12_COMMAND_LIST_TYPE)e;
			queues[e] = new CommandQueueManager(device, type);

			engines[e].frames = new PerEngineInfo::PerFrameInfo[buffers];

			for (int f = 0; f < buffers; f++) {
				PerEngineInfo::PerFrameInfo &frame = engines[e].frames[f];
				frame.allocatorSet = new DX_CommandAllocator[max_threads];
				frame.allocatorsUsed = new bool[max_threads];
				for (int t = 0; t < max_threads; t++)
					// creating gpu allocator for each engine, each frame, and each thread
					device->CreateCommandAllocator(type, IID_PPV_ARGS(&frame.allocatorSet[t]));
			}

			engines[e].queue = queues[e];
			engines[e].threadInfos = new PerEngineInfo::PerThreadInfo[max_threads];
			for (int t = 0; t < max_threads; t++) {
				PerEngineInfo::PerThreadInfo &thread = engines[e].threadInfos[t];

				device->CreateCommandList(0, type, engines[e].frames[0].allocatorSet[0], nullptr, IID_PPV_ARGS(&thread.cmdList));
				thread.cmdList->Close(); // start cmdList state closed.
				thread.isActive = false;

				switch (type) {
				case D3D12_COMMAND_LIST_TYPE_DIRECT:
					thread.manager = new DXRManager(manager, thread.cmdList);
					//thread.manager = new GraphicsManager(manager, thread.cmdList);
					break;
				case D3D12_COMMAND_LIST_TYPE_BUNDLE:
					thread.manager = new GraphicsManager(manager, thread.cmdList);
					break;
				case D3D12_COMMAND_LIST_TYPE_COMPUTE:
					thread.manager = new ComputeManager(manager, thread.cmdList);
					break;
				case D3D12_COMMAND_LIST_TYPE_COPY:
					thread.manager = new CopyingManager(manager, thread.cmdList);
					break;
				}
			}
		}

		// Thread safe concurrent queue for enqueuing work to do asynchronously
		workToDo = new ProducerConsumerQueue<ICallableMember*>(max_threads);

		// Creating worker structs and threads
		workers = new GPUWorkerInfo[max_threads];
		threads = new HANDLE[max_threads];
		counting = new CountEvent();
		for (int i = 0; i < max_threads; i++) {
			workers[i] = { i, this };

			DWORD threadId;
			if (i > 0) // only create threads for workerIndex > 0. Worker 0 will execute on main thread
				threads[i] = CreateThread(nullptr, 0, __WORKER_TODO, &workers[i], 0, &threadId);
		}

		__activeCmdLists = new DX_CommandList[max_threads];

		perFrameFinishedSignal = new Signal[buffers];

		this->threadsCount = max_threads;
	}

	int GPUScheduler::Flush(int mask) {
		counting->Wait();

		int resultMask = 0;

		for (int e = 0; e < CA4G_SUPPORTED_ENGINES; e++)
			if (mask & (1 << e))
			{
				int activeCmdLists = 0;
				for (int t = 0; t < threadsCount; t++) {
					if (engines[e].threadInfos[t].isActive) // pending work here
					{
						engines[e].threadInfos[t].Close();
						__activeCmdLists[activeCmdLists++] = engines[e].threadInfos[t].cmdList;
					}
					auto manager = engines[e].threadInfos[t].manager;

					// Copy all collected descriptors from non-visible to visible DHs.
					if (manager->srcDescriptors.size() > 0)
					{
						this->manager->device->CopyDescriptors(
							manager->dstDescriptors.size(), &manager->dstDescriptors.first(), &manager->dstDescriptorRangeLengths.first(),
							manager->srcDescriptors.size(), &manager->srcDescriptors.first(), nullptr, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
						);
						// Clears the lists for next usage
						manager->srcDescriptors.reset();
						manager->dstDescriptors.reset();
						manager->dstDescriptorRangeLengths.reset();
					}
				}

				if (activeCmdLists > 0) // some cmdlist to execute
				{
					resultMask |= 1 << e;
					queues[e]->Commit(__activeCmdLists, activeCmdLists);
				}
			}

		return resultMask;
	}

	void GPUScheduler::PopulateCommandsBy(ICallableMember *process, int workerIndex) {
		int engineIndex = process->getType();

		auto cmdListManager = engines[engineIndex].threadInfos[workerIndex].manager;

		engines[engineIndex].threadInfos[workerIndex].Activate(engines[engineIndex].frames[currentFrameIndex].RequireAllocator(workerIndex));

		cmdListManager->Tag = process->TagID;
		process->Call(cmdListManager);

		if (workerIndex > 0)
			counting->Signal();
	}

	DWORD WINAPI GPUScheduler::__WORKER_TODO(LPVOID param) {
		GPUWorkerInfo* wi = (GPUWorkerInfo*)param;
		int index = wi->Index;
		GPUScheduler* scheduler = wi->Scheduler;

		while (!scheduler->isClosed()) {
			ICallableMember* nextProcess;
			if (!scheduler->workToDo->TryConsume(nextProcess))
				break;

			scheduler->PopulateCommandsBy(nextProcess, index);

			delete nextProcess;
		}
		return 0;
	}

	void GPUScheduler::PerEngineInfo::PerFrameInfo::ResetUsedAllocators(int threads) {
		for (int i = 0; i < threads; i++)
			//if (allocatorsUsed[i])
		{
			allocatorsUsed[i] = false;
			allocatorSet[i]->Reset();
		}
	}

	void GPUScheduler::PerEngineInfo::PerThreadInfo::Activate(DX_CommandAllocator desiredAllocator) {
		if (isActive)
			return;
		cmdList->Reset(desiredAllocator, nullptr);
		isActive = true;
	}
}