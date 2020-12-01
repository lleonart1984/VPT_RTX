#include "ca4G_Private.h"


namespace CA4G {

	void Technique::__OnInitialization(gObj<DeviceManager> manager) {
		this->manager = manager;
		this->loading = manager->loading;
		this->creating = manager->creating;
		Startup();
		isInitialized = true;
	}
}