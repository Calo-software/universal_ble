package com.navideck.universal_ble

import android.bluetooth.le.ScanFilter
import android.os.ParcelUuid
import java.util.UUID
import kotlin.test.Test
import kotlin.test.assertEquals
import org.mockito.Mockito

internal class NativeServiceDataFilterTest {
    @Test
    fun serviceDataOnlyBeaconsKeepNativePresenceFiltersAsOrAlternatives() {
        val uuids = listOf(
            UUID.fromString("0000181a-0000-1000-8000-00805f9b34fb"),
            UUID.fromString("0000feaa-0000-1000-8000-00805f9b34fb")
        )
        val parcelArguments = mutableListOf<UUID>()
        Mockito.mockConstruction(ParcelUuid::class.java) { _, context ->
            parcelArguments.add(context.arguments()[0] as UUID)
        }.use {
            Mockito.mockConstruction(ScanFilter.Builder::class.java) { builder, _ ->
                Mockito.`when`(builder.setServiceData(Mockito.any(), Mockito.any()))
                    .thenReturn(builder)
                Mockito.`when`(builder.build()).thenReturn(Mockito.mock(ScanFilter::class.java))
            }.use { builders ->
                val filter = UniversalScanFilter(
                    emptyList(), emptyList(), emptyList(), uuids.map { it.toString() }
                )
                val native = filter.toScanFilters(emptyList())
                assertEquals(2, native.size)
                assertEquals(uuids, parcelArguments)
                builders.constructed().forEach { builder ->
                    Mockito.verify(builder).setServiceData(
                        Mockito.any(), Mockito.argThat<ByteArray> { data -> data.isEmpty() }
                    )
                    Mockito.verify(builder).build()
                }
            }
        }
    }
}
